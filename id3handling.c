/*
 * Copyright (c) 2014 Stoian Ivanov
 * Copyright (c) 2009 Chase Douglas
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**********************************************************************************
 * This code is part of HTTP-Live-Video-Stream-Segmenter-and-Distributor
 * look for newer versions at github.com
 **********************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "segmenter.h"

void build_id3_tag(char * id3_tag, size_t max_size) {
	memset (id3_tag,0,max_size);
	
	id3_tag[0] = 'I';
	id3_tag[1] = 'D';
	id3_tag[2] = '3';
	id3_tag[3] = 4;
	id3_tag[9] = '?';
	id3_tag[10] = 'P';
	id3_tag[11] = 'R';
	id3_tag[12] = 'I';
	id3_tag[13] = 'V';
	id3_tag[17] = '5';
	

	strncpy(&id3_tag[20], "com.apple.streaming.transportStreamTimestamp", max_size-20);
	
	
}

void build_image_id3_tag(unsigned char * image_id3_tag, const char *srcfile) {
	if (!srcfile) srcfile=DEFAULT_ID3_TAG_FILE;
	FILE * infile = fopen(srcfile, "rb");
	
	if (!infile) {
		fprintf(stderr, "Could not open audio image id3 tag.");
		exit(0);
	}
	
	fread(image_id3_tag, IMAGE_ID3_SIZE, 1, infile);
	fclose(infile);
}

void fill_id3_tag(char * id3_tag, size_t max_size, unsigned long long pts) {
	id3_tag[max_size - 1] = pts & 0xFF;
	id3_tag[max_size - 2] = (pts >> 8) & 0xFF;
	id3_tag[max_size - 3] = (pts >> 16) & 0xFF;
	id3_tag[max_size - 4] = (pts >> 24) & 0xFF;
	//TODO 33rd bit????
}
