/*
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>

#define segmenter_h_declare_segmenter_c

#include "segmenter.h"
#include <libavformat/avformat.h>
#include <libavutil/opt.h>


#define IMAGE_ID3_SIZE 9171


void ffmpeg_version() {
	printBanner();
    // output build and version numbers
    fprintf(stderr, "libav versions:\n");
    fprintf(stderr, "  libavutil version:   %s\n", AV_STRINGIFY(LIBAVUTIL_VERSION));
    fprintf(stderr, "  libavutil build:     %d\n", LIBAVUTIL_BUILD);
    fprintf(stderr, "  libavcodec version:  %s\n", AV_STRINGIFY(LIBAVCODEC_VERSION));
    fprintf(stdout, "  libavcodec build:    %d\n", LIBAVCODEC_BUILD);
    fprintf(stderr, "  libavformat version: %s\n", AV_STRINGIFY(LIBAVFORMAT_VERSION));
    fprintf(stderr, "  libavformat build:   %d\n", LIBAVFORMAT_BUILD);
	
    fprintf(stderr, "This tool is version " PROGRAM_VERSION ",  built on " __DATE__ " " __TIME__);
#ifdef __GNUC__
    fprintf(stderr, ", with gcc: " __VERSION__ "\n");
#else
    fprintf(stderr, ", using a non-gcc compiler\n");
#endif
}


void build_id3_tag(char * id3_tag, size_t max_size) {
    int i;
    for (i = 0; i < max_size; i++)
        id3_tag[i] = 0;

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

    char id3_tag_name[] = "com.apple.streaming.transportStreamTimestamp";
    strncpy(&id3_tag[20], id3_tag_name, strlen(id3_tag_name));


}

void build_image_id3_tag(unsigned char * image_id3_tag) {
    FILE * infile = fopen("/home/rrtnode/rrt/lib/audio.id3", "rb");

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



static AVStream *add_output_stream(AVFormatContext *output_format_context, AVStream *input_stream) {
    AVCodecContext *input_codec_context;
    AVCodecContext *output_codec_context;
    AVStream *output_stream;

	if (input_stream->codec->codec==NULL){
        fprintf(stderr, "INFO: Opening input sream codec for stream %d\n",input_stream->index);
		AVCodec *codec=avcodec_find_decoder(input_stream->codec->codec_id);
		avcodec_open2(input_stream->codec, codec, NULL);
	}
    output_stream = avformat_new_stream(output_format_context, input_stream->codec->codec);
    if (!output_stream) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }

    input_codec_context = input_stream->codec;
    output_codec_context = output_stream->codec;

    output_codec_context->codec_id = input_codec_context->codec_id;
    output_codec_context->codec_type = input_codec_context->codec_type;
    output_codec_context->codec_tag = input_codec_context->codec_tag;
    output_codec_context->bit_rate = input_codec_context->bit_rate;
    output_codec_context->extradata = input_codec_context->extradata;
    output_codec_context->extradata_size = input_codec_context->extradata_size;

    if (av_q2d(input_codec_context->time_base) * input_codec_context->ticks_per_frame > av_q2d(input_stream->time_base) && av_q2d(input_stream->time_base) < 1.0 / 1000) {
        output_codec_context->time_base = input_codec_context->time_base;
        output_codec_context->time_base.num *= input_codec_context->ticks_per_frame;
    } else {
        output_codec_context->time_base = input_stream->time_base;
    }

    switch (input_codec_context->codec_type) {
        case AVMEDIA_TYPE_AUDIO:

			output_codec_context->channel_layout = input_codec_context->channel_layout;
            output_codec_context->sample_rate = input_codec_context->sample_rate;
            output_codec_context->channels = input_codec_context->channels;
            output_codec_context->frame_size = input_codec_context->frame_size;
            if ((input_codec_context->block_align == 1 && input_codec_context->codec_id == CODEC_ID_MP3) || input_codec_context->codec_id == CODEC_ID_AC3) {
                output_codec_context->block_align = 0;
            } else {
                output_codec_context->block_align = input_codec_context->block_align;
            }
            break;
        case AVMEDIA_TYPE_VIDEO:

			output_codec_context->pix_fmt = input_codec_context->pix_fmt;
            output_codec_context->width = input_codec_context->width;
            output_codec_context->height = input_codec_context->height;
            output_codec_context->has_b_frames = input_codec_context->has_b_frames;

			
			
            break;
        default:
            break;
    }

    return output_stream;
}

int write_index_file(const char *index,  const unsigned int *actual_segment_duration, const char *output_prefix, const char *output_file_extension,  int last_segment) {
	if (last_segment==0) return 0;
	char tmp_index [MAX_FILENAME_LENGTH+5];
	snprintf(tmp_index,MAX_FILENAME_LENGTH+4,"%s.tmp",index);
    FILE *index_fp;
    char write_buf [MAX_FILENAME_LENGTH*2+1];
	int bytes_in_buf;
    unsigned int i;

    index_fp = fopen(tmp_index, "w");
    if (!index_fp) {
        fprintf(stderr, "Could not open temporary m3u8 index file (%s), no index file will be created\n", tmp_index);
        return -1;
    }


    unsigned int maxDuration = actual_segment_duration[0];

    for (i = 1; i <= last_segment; i++) if (actual_segment_duration[i] > maxDuration) maxDuration = actual_segment_duration[i];



	bytes_in_buf=snprintf(write_buf, MAX_FILENAME_LENGTH*2, "#EXTM3U\n#EXT-X-TARGETDURATION:%u\n", maxDuration);

	if (fwrite(write_buf, bytes_in_buf, 1, index_fp) != 1) {
		fprintf(stderr, "Could not write to m3u8 index file, will not continue writing to index file\n");
		fclose(index_fp);
		unlink (tmp_index);
		return -1;
	}

	for (i = 0; i <= last_segment; i++) {
		bytes_in_buf=snprintf(write_buf, MAX_FILENAME_LENGTH*2, "#EXTINF:%u,\n%s-%u%s\n", actual_segment_duration[i], output_prefix, i, output_file_extension);
		if (fwrite(write_buf, bytes_in_buf, 1, index_fp) != 1) {
			fprintf(stderr, "Could not write to m3u8 index file, will not continue writing to index file\n");
			fclose(index_fp);
			unlink (tmp_index);
            return -1;
        }
    }

    bytes_in_buf=snprintf(write_buf, MAX_FILENAME_LENGTH*2, "#EXT-X-ENDLIST\n");
    if (fwrite(write_buf, bytes_in_buf, 1, index_fp) != 1) {
        fprintf(stderr, "Could not write last file and endlist tag to m3u8 index file\n");
        fclose(index_fp);
		unlink (tmp_index);
        return -1;
    }

    fclose(index_fp);

    return rename(tmp_index, index);
}

AVFormatContext * newOutputFile (char *filename, char *format, AVStream* video_in, AVStream* audio_in, AVStream** video_out, AVStream** audio_out) {
	fprintf(stderr, "INFO: new output file %s using muxer %s\n",filename,format);
	int err;
	AVFormatContext * oc=NULL;
	*video_out=NULL;
	*audio_out=NULL;
	if (err=avformat_alloc_output_context2(&oc, NULL,format, filename)<0){
		 fprintf(stderr, "ERROR: Could not initiate output context for %s using muxer %s\n",filename,format);
		 debugReturnCode(err);
		 exit(1);
	};
	if ((err=avio_open2(&oc->pb,filename,AVIO_FLAG_WRITE,NULL,NULL))<0){
		 fprintf(stderr, "ERROR: Could not initiate output file for %s\n",filename);
		 debugReturnCode(err);
		 exit(1);
	};
	if (video_in) {
		*video_out= add_output_stream(oc,video_in);
		if (oc->oformat->flags & AVFMT_GLOBALHEADER)oc->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	if (audio_in) {
		*audio_out=add_output_stream(oc,audio_in);
	}

	av_dump_format(oc, 0, filename, 1);
    return oc;
}


int main(int argc, const char *argv[]) {
    //input parameters
    char inputFilename[MAX_FILENAME_LENGTH], playlistFilename[MAX_FILENAME_LENGTH], baseDirName[MAX_FILENAME_LENGTH], baseFileName[MAX_FILENAME_LENGTH];
    char baseFileExtension[5]; //either "ts", "aac" or "mp3"
    int segmentLength, outputStreams, version,usage,doid3;



    char currentOutputFileName[MAX_FILENAME_LENGTH];
    char tempPlaylistName[MAX_FILENAME_LENGTH];


    unsigned int actual_segment_durations[2048];


    unsigned int output_index = 1;
    AVOutputFormat *ofmt;
    AVFormatContext *ic = NULL;
    AVFormatContext *oc;
    AVStream *out_video_st = NULL;
    AVStream *out_audio_st = NULL;
    AVStream *in_video_st = NULL;
    AVStream *in_audio_st = NULL;
    AVCodec *codec;
    int video_index=-1;
    int audio_index=-1;
    unsigned int first_segment = 1;
    unsigned int last_segment = 0;
    int write_index = 1;
    int decode_done;
    int ret;
    int i;

    unsigned char id3_tag[128];
    unsigned char * image_id3_tag;

	do_streamcopy_opts video_opts, audio_opts;
	memset(&video_opts,0,sizeof(video_opts));
	memset(&audio_opts,0,sizeof(audio_opts));
	
    size_t id3_tag_size = 73;
    int newFile = 1; //a boolean value to flag when a new file needs id3 tag info in it

    if (parseCommandLine(inputFilename, playlistFilename, baseDirName, baseFileName, baseFileExtension, &outputStreams, &segmentLength, &verbosity, &version,&usage, &doid3,  argc, argv) != 0)
        return 0;

	if (usage) printUsage();
    if (version) ffmpeg_version();
	if (version || usage) return 0;


    //fprintf(stderr, "%s %s\n", playlistFilename, tempPlaylistName);

	if (doid3) {
		image_id3_tag = malloc(IMAGE_ID3_SIZE);
		if (outputStreams == OUTPUT_STREAM_AUDIO) build_image_id3_tag(image_id3_tag);
		build_id3_tag((char *) id3_tag, id3_tag_size);
	}

    snprintf(playlistFilename, MAX_FILENAME_LENGTH, "%s/%s", baseDirName, playlistFilename);


	

    av_log_set_level(verbosity);

    av_register_all();

	AVBitStreamFilterContext* bsf_h264_mp4toannexb= NULL;
	
	ret = avformat_open_input(&ic, inputFilename, NULL, NULL);
    if (ret != 0) {
        fprintf(stderr, "ERROR: Could not open input file %s. Error %d.\n", inputFilename, ret);
        exit(1);
    }

    if (avformat_find_stream_info(ic, NULL) < 0) {
        fprintf(stderr, "ERROR: Could not read stream information.\n");
        exit(1);
    }

    av_dump_format(ic, 0, inputFilename, 0);

    for (i = 0; i < ic->nb_streams && (video_index < 0 || audio_index < 0); i++) {
        switch (ic->streams[i]->codec->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
				video_index = i;
				ic->streams[i]->discard = AVDISCARD_NONE;
				if (outputStreams & OUTPUT_STREAM_VIDEO) in_video_st=ic->streams[i];
                break;
            case AVMEDIA_TYPE_AUDIO:
                audio_index = i;
                ic->streams[i]->discard = AVDISCARD_NONE;
                if (outputStreams & OUTPUT_STREAM_AUDIO) in_audio_st=ic->streams[i];
                break;
            default:
                ic->streams[i]->discard = AVDISCARD_ALL;
                break;
        }
    }

    //now that we know the audio and video output streams
    //we can decide on an output format.
    char *output_format;
    if (outputStreams == OUTPUT_STREAM_AUDIO) {
        //the audio output format should be the same as the audio input format
        switch (in_audio_st->codec->codec_id) {
            case CODEC_ID_MP3:
                fprintf(stderr, "Setting output audio to mp3.");
                strcpy(baseFileExtension, ".mp3");
				output_format="mp3";
                break;
            case CODEC_ID_AAC:
                fprintf(stderr, "Setting output audio to aac.");
                strcpy(baseFileExtension, ".aac");
                output_format="adts";
                break;
            default:
                fprintf(stderr, "Codec id %d not supported.\n", in_audio_st->codec->codec_id);
				exit(1);
        }
    } else {
        output_format="mpegts";
    }
    
    
    snprintf(currentOutputFileName, MAX_FILENAME_LENGTH, "%s/%s-%u%s", baseDirName, baseFileName, output_index++, baseFileExtension);

	
	oc=newOutputFile(currentOutputFileName,output_format,in_video_st,in_audio_st,&out_video_st,&out_audio_st);
    newFile = 1;
	
	video_opts.skiptokeyframe=1;
	audio_opts.skiptokeyframe=1;
	video_opts.interleave_write=(outputStreams==(OUTPUT_STREAM_VIDEO|OUTPUT_STREAM_AUDIO));
	audio_opts.interleave_write=video_opts.interleave_write;
	
    int r = avformat_write_header(oc, NULL);
    if (r) {
        fprintf(stderr, "ERROR: Could not write header to first output file.\n");
        debugReturnCode(r);
        exit(1);
    }


	double input_time_constant_v =  (video_index==-1?0:(double)ic->streams[video_index]->time_base.num / ic->streams[video_index]->time_base.den);
	double input_time_constant_a =  (audio_index==-1?0:(double)ic->streams[audio_index]->time_base.num / ic->streams[audio_index]->time_base.den);
	int is_video,is_audio;
	double segment_start=-1;
	double input_time=0;
    double current_segment_length=0;
    double current_segment_length_up_to_this_packet=0;
	int64_t in_video_pts_base=AV_NOPTS_VALUE;
	int64_t in_audio_pts_base=AV_NOPTS_VALUE;
	
	//we break the input over keyframes from which stream:
	int sync_stream_index=(outputStreams & OUTPUT_STREAM_VIDEO?video_index:audio_index);
    do {
        AVPacket packet;

        decode_done = av_read_frame(ic, &packet);

        if (decode_done < 0) {
            break;
        }
		is_video=(packet.stream_index == video_index );
		is_audio=(packet.stream_index == audio_index ); 
		if (!(is_video || is_audio)) continue;
		if (is_video && ((outputStreams & OUTPUT_STREAM_VIDEO)==0)) continue;
		if (is_audio && ((outputStreams & OUTPUT_STREAM_AUDIO)==0)) continue;
		
        if ( packet.pts!=AV_NOPTS_VALUE)
        {
            if (is_video) {
				if (in_video_pts_base==AV_NOPTS_VALUE) in_video_pts_base=packet.pts;
				input_time = (double) (packet.pts - in_video_pts_base)  * input_time_constant_v; //we do video output (we have video frame at this point) we calculate time based on it
			} else {
				if (in_audio_pts_base==AV_NOPTS_VALUE) in_audio_pts_base=packet.pts;
				if ((outputStreams & OUTPUT_STREAM_VIDEO)==0) input_time = (double) (packet.pts - in_audio_pts_base) * input_time_constant_a; //we do audio output and we don't do video so base time on audio
			}
			if (segment_start==-1) segment_start=input_time;
			current_segment_length_up_to_this_packet=current_segment_length;
			current_segment_length=input_time-segment_start;
		}

		fprintf(stderr, "DEBUG: reading: v:%d a:%d kf:%d pts:%lld (%lld)  it:%lf\n", is_video, is_audio, (packet.flags & AV_PKT_FLAG_KEY)!=0, packet.pts,packet.pts-(is_audio?in_audio_pts_base:in_video_pts_base), input_time);
		
        //start looking for segment splits for videos one half second before segment duration expires. This is because the 
        //segments are split on key frames so we cannot expect all segments to be split exactly equally. 
        if ( packet.stream_index==sync_stream_index && current_segment_length_up_to_this_packet  >= segmentLength - 0.5 && (packet.flags & AV_PKT_FLAG_KEY)) {
            actual_segment_durations[++last_segment] = current_segment_length_up_to_this_packet;
			fprintf(stderr, "INFO: Segment %d has duration time %lf\n", last_segment, current_segment_length_up_to_this_packet);
			av_write_trailer(oc);
            avio_flush(oc->pb);
            avio_close(oc->pb);
			avformat_free_context(oc);
			

            snprintf(currentOutputFileName, MAX_FILENAME_LENGTH, "%s/%s-%u%s", baseDirName, baseFileName, output_index++, baseFileExtension);
			
			oc=newOutputFile(currentOutputFileName,output_format,in_video_st,in_audio_st,&out_video_st,&out_audio_st);
            newFile = 1;
			video_opts.skiptokeyframe=1;
			audio_opts.skiptokeyframe=1;
        }

        if (is_audio) {
            if (newFile && outputStreams == OUTPUT_STREAM_AUDIO) {
                //add id3 tag info
                //fprintf(stderr, "adding id3tag to file %s\n", currentOutputFileName);
                //printf("%lf %lld %lld %lld %lld %lld %lf\n", segment_time, audio_st->pts.val, audio_st->cur_dts, audio_st->cur_pkt.pts, packet.pts, packet.dts, packet.dts * av_q2d(ic->streams[audio_index]->time_base) );
                if (doid3) {
					fill_id3_tag((char*) id3_tag, id3_tag_size, packet.dts);
					avio_write(oc->pb, id3_tag, id3_tag_size);
					avio_write(oc->pb, image_id3_tag, IMAGE_ID3_SIZE);
					avio_flush(oc->pb);
				}
                newFile = 0;
            }

            do_streamcopy (in_audio_st,out_audio_st,oc,&packet,&audio_opts);
        } else if (is_video) {
            if (newFile) {
                //fprintf(stderr, "New File: %lld %lld %lld\n", packet.pts, video_st->pts.val, audio_st->pts.val);
                //printf("%lf %lld %lld %lld %lld %lld %lf\n", segment_time, audio_st->pts.val, audio_st->cur_dts, audio_st->cur_pkt.pts, packet.pts, packet.dts, packet.dts * av_q2d(ic->streams[audio_index]->time_base) );
                newFile = 0;
            }
            
            
            do_streamcopy (in_video_st,out_video_st,oc,&packet,&video_opts);
        }

        if (ret < 0) {
            fprintf(stderr, "Warning: Could not write frame of stream.\n");
        } else if (ret > 0) {
            fprintf(stderr, "End of stream requested.\n");
            av_free_packet(&packet);
            break;
        }

        av_free_packet(&packet);
    } while (!decode_done);

    //make sure all packets are written and then close the last file. 
    av_write_trailer(oc);
    avio_flush(oc->pb);
	avio_close(oc->pb);
	avformat_free_context(oc);


	actual_segment_durations[++last_segment] = (unsigned int) rint(current_segment_length);

	if (actual_segment_durations[last_segment] == 0)     actual_segment_durations[last_segment] = 1;

	write_index_file(playlistFilename, actual_segment_durations,  baseFileName, baseFileExtension, last_segment);


    return 0;
}
