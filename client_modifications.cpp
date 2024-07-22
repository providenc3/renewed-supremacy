#include "includes.h"

void __fastcall madik_hooks::drawprinttext_hk(void* ecx, void* edx, const wchar_t* text, int text_length, void* draw_type) {
	if (text_length < 10)
		return madik_hooks::o_drawprinttext(ecx, edx, text, text_length, draw_type);



	if ((text[0] == L'f' && text[1] == L'p' && text[2] == L's') || (text[0] == L'l' && text[1] == L'o' && text[2] == L's')) {
		std::wstring appender;

		if (text[0] == L'f')
			appender = L"";
		else if (text[0] == L'l') {
			appender = L"madikhook";
		}

		return madik_hooks::o_drawprinttext(ecx, edx, appender.data(), appender.size(), draw_type);
	}

	madik_hooks::o_drawprinttext(ecx, edx, text, text_length, draw_type);
}