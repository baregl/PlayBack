/*
 * From PKGj
 *
 * Copyright 2018 Philippe Daouadi, 2018 baregl
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"

#include "constants.h"
#include "syncer.h"
#include <strings.h>

char *skipnonws(char *text, char *end)
{
	while (text < end && *text != ' ' && *text != '\n' && *text != '\r') {
		text++;
	}
	return text;
}

char *skipws(char *text, char *end)
{
	while (text < end && (*text == ' ' || *text == '\n' || *text == '\r')) {
		text++;
	}
	return text;
}

int config_parse(char *file)
{
	LOG("Opening config\n");
	clbk_open(file);
	// For the final NULL Byte
	char buffer[config_size + 1];
	char *text = (char *)buffer;
	LOG("Reading config\n");
	uint16_t read_len = clbk_read((uint8_t *)buffer, config_size - 1);
	LOG("Read config\n");
	if (read_len == config_size)
		// Too long
		return -2;
	LOG("Writing final newline at %i with maximum length %i\n", read_len,
	    config_size);
	buffer[read_len] = '\n';
	LOG("Finding eof\n");
	char *end = (char *)buffer + read_len + 1;

	LOG("Parsing config\n");
	while (text < end) {
		char *key = text;

		text = skipnonws(text, end);
		if (text == end)
			return -1;

		*text++ = 0;

		text = skipws(text, end);
		if (text == end)
			return -1;

		char *value = text;

		text = skipnonws(text, end);

		*text++ = 0;
		clbk_config_entry(key, value);
		if (text == (end + 1)) {
			break;
		}

		text = skipws(text, end);
	}
	return 0;
}
