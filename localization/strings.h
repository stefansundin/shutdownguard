#include "en-US/strings.h"
#include "es-ES/strings.h"
#include "lt-LT/strings.h"
#include "nn-NO/strings.h"

struct {
	wchar_t *code;
	struct strings *strings;
} languages[] = {
	{L"en-US", &en_US},
	{L"es-ES", &es_ES},
	{L"lt-LT", &lt_LT},
	{L"nn-NO", &nn_NO},
};

int num_languages = 4;
