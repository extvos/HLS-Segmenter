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
#ifndef segmenter_h_a22d54e85a2948bbb2c2219e34c7c168_included
#define segmenter_h_a22d54e85a2948bbb2c2219e34c7c168_included

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdlib.h>

#define PROGRAM_VERSION "0.2"

#define MAX_SEGMENTS 2048
#define MAX_FILENAME_LENGTH 1024
#define MAXT_EXT_LENGTH 9
//type of output to perform
#define OUTPUT_STREAM_AUDIO 1
#define OUTPUT_STREAM_VIDEO 2
#define OUTPUT_STREAM_AV (OUTPUT_STREAM_AUDIO | OUTPUT_STREAM_VIDEO)

//SDR: don't ask! I don't know either 
#define IMAGE_ID3_SIZE 9171
#define DEFAULT_ID3_TAG_FILE "/home/rrtnode/rrt/lib/audio.id3"
	
//options_parsing.c:
void printBanner();
void printUsage();
int parseCommandLine(char * inputFile, char * outputFile, char * baseDir, char * baseName, char * baseExtension, int * outputStreams, int * segmentLength, int * verbosity, int * version, int * usage,int * doid3tag, int argc, const char * argv[]);

//id3handling.c:
void build_id3_tag(char * id3_tag, size_t max_size);
void build_image_id3_tag(unsigned char * image_id3_tag, const char *srcfile);
void fill_id3_tag(char * id3_tag, size_t max_size, unsigned long long pts);


//helpers.c:
void ffmpeg_version();
void debugReturnCode(int r);

#define FNHOLDER(name) char name[MAX_FILENAME_LENGTH+1];name[MAX_FILENAME_LENGTH]=0;

#ifdef  __cplusplus
}
#endif
#endif