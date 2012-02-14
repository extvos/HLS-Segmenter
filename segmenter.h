/*
 * Copyright (c) 2009 Chase Douglas
 * Copyright (c) 2012 Stoian Ivanov 
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
#include <stdint.h>
#include <libavformat/avformat.h>


#define PROGRAM_VERSION "0.1"

#define MAX_FILENAME_LENGTH 1024

//type of output to perform
#define OUTPUT_STREAM_AUDIO 1
#define OUTPUT_STREAM_VIDEO 2
#define OUTPUT_STREAM_AV (OUTPUT_STREAM_AUDIO | OUTPUT_STREAM_VIDEO)

#ifdef  __cplusplus
}
#endif

//options_parsing.c
void printBanner();
void printUsage();
int parseCommandLine(char * inputFile, char * outputFile, char * baseDir, char * baseName, char * baseExtension, int * outputStreams, int * segmentLength, int * verbosity, int * version, int * usage,int * doid3tag, int argc, const char * argv[]);

//debugs.c:
void debugReturnCode(int r);

//avconv_adapted_code.c

 
typedef struct do_streamcopy_opts {
	int64_t ost_tb_start_time;
	int64_t last_dts;
	int64_t last_pts;
	int skiptokeyframe;
	AVBitStreamFilterContext * bitstream_filters;
	int interleave_write;
} do_streamcopy_opts;

#ifdef segmenter_h_declare_segmenter_c
int verbosity;
#else 
extern int verbosity; 
#endif

void write_frame(AVFormatContext *s, AVPacket *pkt, AVStream *o_st, do_streamcopy_opts* opts);
void do_streamcopy(AVStream *i_st, AVStream *o_st, AVFormatContext *o_fctx, const AVPacket *pkt, do_streamcopy_opts* opts);


#endif