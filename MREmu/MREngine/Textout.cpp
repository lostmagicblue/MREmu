#include "Graphic.h"
#include "Canvas.h"
#include "../Memory.h"
#include <vmgraph.h>
#include <vmstdlib.h>
#include "unifont.h"
#include <iostream>
#include <cstring>
#include <string>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Font.hpp>
#include <algorithm>
#include <vector>
#include <unordered_map>

extern MREngine::Graphic* graphic;

// ================================================================================
// 可选 TTF 字体层：把中文 TTF 改名 cjk.ttf（或 .ttc/.otf）放进 fonts/ 目录即全局生效，
// 屏幕文字变平滑 TrueType（带抗锯齿 alpha 混合）；没有这个文件时自动回退内置
// unifont 16px 点阵，行为和以前完全一样。fonts/ 会被自动拷到 bin/fonts/（copy_fonts 目标）。
// ================================================================================
namespace {
	const int TTF_PX = 16;       // 渲染字号（和点阵行高一致，不破坏现有排版）
	const int TTF_BASELINE = 14; // 16px 行内的基线位置（CJK 字面通常正好占 0..16）

	struct TtfGlyph {
		bool valid = false;          // TTF 里有这个字
		int  w = 0, h = 0;           // 位图尺寸（空格等无位图字符 w=h=0）
		int  off_x = 0;              // 相对笔位置的 x 偏移
		int  off_y = 0;              // 相对行顶的 y 偏移（= TTF_BASELINE + bounds.top）
		int  advance = 0;            // 推进宽度
		std::vector<uint8_t> alpha;  // 8bit 覆盖度，size = w*h
	};

	sf::Font* g_ttf = nullptr;
	bool g_ttf_tried = false;
	std::unordered_map<uint32_t, TtfGlyph> g_ttf_cache;

	sf::Font* ttf_get() {
		if (g_ttf_tried)
			return g_ttf;
		g_ttf_tried = true;

		const char* candidates[] = {
			"fonts/cjk.ttf",     "fonts/cjk.ttc",     "fonts/cjk.otf",
			"../fonts/cjk.ttf",  "../fonts/cjk.ttc",  "../fonts/cjk.otf",
			"../../fonts/cjk.ttf",
			"cjk.ttf", "cjk.ttc", "cjk.otf",
		};
		for (auto* p : candidates) {
			auto* f = new sf::Font();
			if (f->loadFromFile(p)) {
				g_ttf = f;
				break;
			}
			delete f;
		}
		return g_ttf;
	}

	bool ttf_ready() {
		return ttf_get() != nullptr;
	}

	TtfGlyph& ttf_glyph(VMWCHAR c) {
		auto it = g_ttf_cache.find(c);
		if (it != g_ttf_cache.end())
			return it->second;

		TtfGlyph glyph;
		if (sf::Font* f = ttf_get()) {
			// 字体里没有这个字：valid 保持 false → 上层逐字回退点阵。
			// 不能直接 getGlyph——缺字时它返回替换字形（空心方块），回退会失效。
			if (f->hasGlyph((sf::Uint32)c)) {
				sf::Glyph g = f->getGlyph((sf::Uint32)c, TTF_PX, false);
				glyph.advance = (int)(g.advance + 0.5f);

				// 版本无关取位图：sf::Glyph::texture 只有 SFML 2.6/3.x 才有，
				// 统一用 font.getTexture(字号) 的字集图集 + textureRect 抠出本字形区域。
				sf::IntRect tr = g.textureRect;
				if (tr.width > 0 && tr.height > 0) {
					sf::Image img = f->getTexture(TTF_PX).copyToImage();
					int img_w = (int)img.getSize().x;

					glyph.w = tr.width;
					glyph.h = tr.height;
					glyph.off_x = (int)g.bounds.left;
					glyph.off_y = TTF_BASELINE + (int)g.bounds.top;
					glyph.alpha.resize(glyph.w * glyph.h);

					const uint8_t* base = img.getPixelsPtr();
					for (int py = 0; py < tr.height; ++py)
						for (int px = 0; px < tr.width; ++px)
							glyph.alpha[py * tr.width + px] =
								base[(((size_t)tr.top + py) * img_w + (tr.left + px)) * 4 + 3];

					glyph.valid = true;
				}
				else if (glyph.advance > 0) {
					glyph.valid = true;   // 空格这类无位图但有推进宽的字符
				}
			}
		}
		return g_ttf_cache.emplace((uint32_t)c, std::move(glyph)).first->second;
	}

	// 565 空间直接做 alpha 混合（a: 0-255），免 888 往返
	inline VMUINT16 ttf_blend(VMUINT16 dst, VMUINT16 col, int a) {
		int dr = (dst >> 11) & 0x1F, dg = (dst >> 5) & 0x3F, db = dst & 0x1F;
		int cr = (col >> 11) & 0x1F, cg = (col >> 5) & 0x3F, cb = col & 0x1F;
		dr += (cr - dr) * a / 255;
		dg += (cg - dg) * a / 255;
		db += (cb - db) * a / 255;
		return (VMUINT16)((dr << 11) | (dg << 5) | db);
	}
}

sf::Texture u16text_to_texture(std::u16string str, sf::Color c) {
	sf::Image im;

	// —— TTF 层：加载了 fonts/cjk.ttf 时，走平滑 TrueType 字形 ——
	if (ttf_ready()) {
		int w = 0;
		for (int i = 0; i < str.length(); ++i)
			w += ttf_glyph(str[i]).advance;
		if (w < 1) w = 1;

		const int tex_h = 18;   // 16 行 + 下伸部余量
		im.create(w, tex_h, sf::Color::Transparent);
		sf::Color* buf32_dst = (sf::Color*)im.getPixelsPtr();

		int x_off = 0;
		for (int i = 0; i < str.length(); ++i) {
			TtfGlyph& g = ttf_glyph(str[i]);
			if (!g.valid) continue;

			for (int py = 0; py < g.h; ++py) {
				int sy = g.off_y + py;
				if (sy < 0 || sy >= tex_h) continue;
				for (int px = 0; px < g.w; ++px) {
					int sx = x_off + g.off_x + px;
					if (sx < 0 || sx >= w) continue;
					int a = g.alpha[py * g.w + px];
					if (!a) continue;
					sf::Color d = buf32_dst[sy * w + sx];
					auto ch = [&](int dc, int cc) { return (uint8_t)(dc + (cc - dc) * a / 255); };
					buf32_dst[sy * w + sx] = sf::Color(
						ch(d.r, c.r), ch(d.g, c.g), ch(d.b, c.b),
						(uint8_t)std::max<int>(d.a, a));
				}
			}
			x_off += g.advance;
		}

		sf::Texture tex;
		tex.loadFromImage(im);
		return tex;
	}

	int w = vm_graphic_get_string_width((VMWSTR)str.c_str());
	im.create(w, 16, sf::Color::Transparent);

	sf::Color* buf32_dst = (sf::Color*)im.getPixelsPtr();

	int st_y = 0;
	int end_y = 16;

	int x_off = 0;
	for (int i = 0; i < str.length(); ++i) {
		int data_offset = ((unsigned int*)unifont_15_1_04_bin)[(unsigned short)str[i]];

		// 缺字：宽度和绘制必须同步跳过（vm_graphic_get_string_width 里同样 continue）
		if (data_offset == 0)
			continue;

		int ch_d = unifont_15_1_04_bin[data_offset];
		int ch_w = ch_d & 0xF;
		bool sho = ch_w >= 8;

		if (x_off >= w)
			break;

		// 关键：每个字符只写自己的 [x_off, x_off+ch_w] 区域。
		// 之前从 0 开始写，第二个字起 im_x 为负 → 移位 UB → 把前面已画的字覆盖成乱码
		//（表现为「菜单」只剩半个「菜」）。
		int end_x = std::min<int>(w, x_off + ch_w + 1);

		for (int sy = st_y; sy < end_y; ++sy) {
			int tex_ty = sy;
			unsigned short line = 0;
			if (sho)
				line = (unifont_15_1_04_bin[data_offset + 2 + tex_ty * 2] << 8) |
				unifont_15_1_04_bin[data_offset + 2 + tex_ty * 2 + 1];
			else
				line = unifont_15_1_04_bin[data_offset + 2 + tex_ty] << 8;

			for (int sx = x_off; sx < end_x; ++sx) {
				int im_x = sx - x_off;

				if ((line >> (15 - im_x)) & 1)
					buf32_dst[sy * w + sx] = c;
			}
		}

		x_off += ch_w + 1;
	}

	sf::Texture tex;
	tex.loadFromImage(im);
	return tex;
}

static bool is_skip_symbol(VMWCHAR c) {
	return c < 0x20 || c == 0x7F;
}

VMINT vm_graphic_get_character_height(void) {
	return 16; // temp
}

VMINT vm_graphic_get_character_width(VMWCHAR c) {
	if (is_skip_symbol(c))
		return 0;

	// TTF 层：宽度用字形推进宽；TTF 缺字则继续往下用点阵宽（和 textout 的逐字回退保持一致）
	if (ttf_ready() && ttf_glyph(c).valid)
		return ttf_glyph(c).advance;

	int data_offset = ((unsigned int*)unifont_15_1_04_bin)[(unsigned short)c];

	if (data_offset == 0)
		return 0;

	int ch_d = unifont_15_1_04_bin[data_offset];
	int ch_w = ch_d & 0xF;
	return ch_w + 1;
}

VMINT vm_graphic_get_string_width(VMWSTR str) {
	if (!str)
		return 0;
	int w = 0;

	// TTF 层：有效字形用推进宽，缺字逐字回退点阵宽（和 textout 保持一致）；保留原有 +1 余量
	if (ttf_ready()) {
		for (int i = 0; str[i]; ++i) {
			if (is_skip_symbol(str[i]))
				continue;
			TtfGlyph& g = ttf_glyph(str[i]);
			w += g.valid ? g.advance : vm_graphic_get_character_width(str[i]);
		}
		return w + 1;
	}

	for (int i = 0; str[i]; ++i) {
		int data_offset = ((unsigned int*)unifont_15_1_04_bin)[(unsigned short)str[i]];

		if (data_offset == 0 || is_skip_symbol(str[i]))
			continue;

		int ch_d = unifont_15_1_04_bin[data_offset];
		int ch_w = ch_d & 0xF;

		w += ch_w + 1;
	}
	return w+1;
}

VMINT vm_graphic_get_string_height(VMWSTR str) {
	return vm_graphic_get_character_height();
}

VMINT vm_graphic_measure_character(VMWCHAR c, VMINT* width, VMINT* height) {
	if (width == 0 || height == 0)
		return VM_GDI_FAILED;

	*width = vm_graphic_get_character_width(c);
	*height = vm_graphic_get_character_height();

	return VM_GDI_SUCCEED;
}

VMINT vm_graphic_get_character_info(VMWCHAR c, vm_graphic_char_info* char_info) {
	if (char_info == 0)
		return -1;

	// TTF 层：宽度/基线来自字形
	if (!is_skip_symbol(c) && ttf_ready()) {
		TtfGlyph& g = ttf_glyph(c);
		if (!g.valid)
			return -1;
		char_info->dwidth = g.advance;
		char_info->width = g.advance;
		char_info->height = 16;
		char_info->ascent = TTF_BASELINE;
		char_info->descent = 16 - TTF_BASELINE;
		return 0;
	}

	unsigned int data_offset = ((unsigned int*)unifont_15_1_04_bin)[(unsigned short)c];

	if (data_offset == 0 || is_skip_symbol(c))
		return -1;

	int ch_d = unifont_15_1_04_bin[data_offset];
	int ch_w = ch_d & 0xF;

	char_info->dwidth = ch_w - (ch_d >> 4);
	char_info->width = ch_w + 1;
	char_info->height = 16;
	char_info->ascent = 2;
	char_info->descent = 0;

	return 0;
}

void vm_graphic_set_font(font_size_t size) {
}

void vm_graphic_textout(VMUINT8* disp_buf, VMINT x, VMINT y, VMWSTR s, VMINT length, VMUINT16 color) {
	if (disp_buf == 0 || !s)
		return;

	MREngine::canvas_signature* cs_dst = (MREngine::canvas_signature*)(disp_buf - VM_CANVAS_DATA_OFFSET);
	if (memcmp(cs_dst->magic, CANVAS_MAGIC, 9))
		return;
	MREngine::canvas_frame_property* cfp_dst = (MREngine::canvas_frame_property*)(cs_dst + 1);
	unsigned short* buf16_dst = (unsigned short*)(cfp_dst + 1);

	int left = 0;
	int top = 0;
	int right = cfp_dst->width;
	int bottom = cfp_dst->height;

	auto& clip = get_current_app_graphic().clip;
	if (clip.flag) {
		if (left < clip.left)
			left = clip.left;
		if (top < clip.top)
			top = clip.top;
		if (right > clip.right + 1)
			right = clip.right + 1;
		if (bottom > clip.bottom + 1)
			bottom = clip.bottom + 1;
	}

	int st_y = std::max(top, y);
	int end_y = std::min<int>(bottom, y + 16);
	// TTF 字形带抗锯齿边缘和下伸部，允许向下多画 2px（点阵路径仍按 16 行裁剪）
	int ttf_end_y = std::min<int>(bottom, y + 18);

	int x_off = x;
	for (int i = 0; i < length && s[i]; ++i) {
		// —— TTF 层：有字体且该字符在 TTF 里，走平滑字形 alpha 混合 ——
		if (!is_skip_symbol(s[i]) && ttf_ready()) {
			TtfGlyph& g = ttf_glyph(s[i]);
			if (g.valid) {
				int gx = x_off + g.off_x;
				int gy = y + g.off_y;
				for (int py = 0; py < g.h; ++py) {
					int sy = gy + py;
					if (sy < st_y || sy >= ttf_end_y) continue;
					const uint8_t* row = &g.alpha[py * g.w];
					unsigned short* dst_row = buf16_dst + sy * cfp_dst->width;
					for (int px = 0; px < g.w; ++px) {
						int sx = gx + px;
						if (sx < left || sx >= right) continue;
						int a = row[px];
						if (a)
							dst_row[sx] = ttf_blend(dst_row[sx], color, a);
					}
				}
				x_off += g.advance;
				continue;
			}
		}

		// —— unifont 点阵（无 TTF 或该字符 TTF 缺字时逐字回退）——
		int data_offset = ((unsigned int*)unifont_15_1_04_bin)[(unsigned short)s[i]];

		if (data_offset == 0 || is_skip_symbol(s[i]))
			continue;

		int ch_d = unifont_15_1_04_bin[data_offset];
		int ch_w = ch_d & 0xF;
		bool sho = ch_w >= 8;

		if (x_off >= right)
			break;

		if (x_off + ch_w < left) {
			x_off += ch_w + 1;
			continue;
		}

		int st_x = std::max(left, x_off);
		int end_x = std::min<int>(right, x_off + ch_w + 1);

		for (int sy = st_y; sy < end_y; ++sy) {
			int tex_ty = sy - y;
			unsigned short line = 0;
			if (sho)
				line = (unifont_15_1_04_bin[data_offset + 2 + tex_ty * 2] << 8) |
				unifont_15_1_04_bin[data_offset + 2 + tex_ty * 2 + 1];
			else
				line = unifont_15_1_04_bin[data_offset + 2 + tex_ty] << 8;

			for (int sx = st_x; sx < end_x; ++sx) {
				int im_x = sx - x_off;

				if ((line >> (15 - im_x)) & 1)
					buf16_dst[sy * cfp_dst->width + sx] = color;
			}
		}

		x_off += ch_w + 1;
	}
}

void vm_graphic_textout_by_baseline(VMUINT8* disp_buf, VMINT x, VMINT y, VMWSTR s, VMINT length, VMUINT16 color, VMINT baseline) {
	vm_graphic_textout(disp_buf, x, y, s, length, color); //TODO
}

VM_GDI_RESULT vm_font_set_font_size(VMINT size) {
	return VM_GDI_SUCCEED;
}

VM_GDI_RESULT vm_font_set_font_style(VMINT bold, VMINT italic, VMINT underline) {
	return VM_GDI_SUCCEED;
}

VM_GDI_RESULT vm_graphic_textout_to_layer(VMINT handle, VMINT x, VMINT y, VMWSTR s, VMINT length) {
	auto& layers = get_current_app_graphic().layers;

	if (handle < 0 || handle >= layers.size())
		return VM_GDI_FAILED;

	auto& layer = layers[handle];

	vm_graphic_textout((VMUINT8*)layer.buf, x, y, s, length, get_current_app_graphic().global_color.vm_color_565);

	return VM_GDI_SUCCEED;
}

VMINT vm_graphic_get_string_baseline(VMWSTR string) {
	return vm_graphic_get_string_height(string) - 2;
}

VM_GDI_RESULT vm_graphic_textout_to_layer_by_baseline(VMINT handle, VMINT x, VMINT y, VMWSTR s, VMINT length, VMINT baseline);

VMINT vm_graphic_is_use_vector_font(void) {
	return FALSE;
}

VM_GDI_RESULT vm_graphic_draw_abm_text(VMINT handle, VMINT x, VMINT y, VMINT color, VMUINT8* font_data, VMINT font_width, VMINT font_height);

VMUINT vm_graphic_get_char_num_in_width(VMWCHAR* string, VMUINT width, VMINT  checklinebreak, VMUINT gap) {
	// TTF 层：按字形推进宽累计
	if (ttf_ready()) {
		int w = 0, i = 0;
		for (i = 0; string[i]; ++i) {
			if (is_skip_symbol(string[i]))
				continue;
			int cw = ttf_glyph(string[i]).advance;
			if (width >= w && width < w + cw + gap)
				return i;
			w += cw + gap;
		}
		return i;
	}

	int w = 0, i = 0;
	for (i = 0; string[i]; ++i) {
		int data_offset = ((unsigned int*)unifont_15_1_04_bin)[(unsigned short)string[i]];

		if (data_offset == 0 || is_skip_symbol(string[i]))
			continue;

		int ch_d = unifont_15_1_04_bin[data_offset];
		int ch_w = ch_d & 0xF;

		if (width >= w && width < w + ch_w + 1 + gap)
			return i;

		w += ch_w + 1 + gap;
	}
	return i;
}

VMUINT vm_graphic_get_char_num_in_width_ex(VMWCHAR* string, VMUINT width, VMINT  checklinebreak, VMUINT gap);

VMUINT vm_get_string_width_height_ex(
	VMWCHAR* string,
	VMINT gap,
	VMINT n,
	VMINT* pWidth,
	VMINT* pHeight,
	VMINT max_width,
	VMUINT8 checkLineBreak,
	VMUINT8 checkCompleteWord);

vm_font_engine_error_message_enum vm_graphic_show_truncated_text(VM_GDI_HANDLE dest_layer_handle,
	VMINT x,
	VMINT y,
	VMINT xwidth,
	VMWCHAR* st,
	VMWCHAR* truncated_symbol,
	VMINT bordered,
	VMUINT16 color) {

	auto& layers = get_current_app_graphic().layers;

	if (dest_layer_handle < 0 || dest_layer_handle >= layers.size())
		return VM_FONT_ENGINE_ERROR;

	if (!st)
		return VM_FONT_ENGINE_ERROR;

	auto& layer = layers[dest_layer_handle];

	if (vm_graphic_get_string_width(st) <= xwidth) {
		vm_graphic_textout((VMUINT8*)layer.buf, x, y, st, vm_wstrlen(st), color);
		return VM_FONT_ENGINE_NO_ERROR;
	}

	int truncated_w = 0;
	int truncated_i = 0;
	if (truncated_symbol) {
		for (truncated_i = 0; truncated_symbol[truncated_i]; ++truncated_i) {
			int char_w = vm_graphic_get_character_width(truncated_symbol[truncated_i]);
			if (truncated_w + char_w > xwidth)
				break;
			truncated_w += char_w;
		}
	}

	int text_w = 0;
	int text_i = 0;
	for (text_i = 0; st[text_i]; ++text_i) {
		int char_w = vm_graphic_get_character_width(st[text_i]);
		if (text_w + char_w + truncated_w > xwidth)
			break;
		text_w += char_w;
	}

	vm_graphic_textout((VMUINT8*)layer.buf, x, y, st, text_i, color);
	if (truncated_symbol)
		vm_graphic_textout((VMUINT8*)layer.buf, x + text_w, y, truncated_symbol, truncated_i, color);

	return VM_FONT_ENGINE_NO_ERROR;
}

VMINT vm_graphic_get_character_height_ex(VMUWCHAR c);

VMINT vm_graphic_get_highest_char_height_of_all_language(void);

VMINT vm_graphic_get_char_height_alllang(VMINT size);

VMINT vm_graphic_get_char_baseline_alllang(VMINT size);
