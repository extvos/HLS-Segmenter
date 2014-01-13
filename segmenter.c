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
#include <sys/stat.h>

#include "segmenter.h"
#include "libavformat/avformat.h"



void write_stream_size_file(const char file_directory[], const char filename_prefix[], double size) {
    FILE * outputFile;
    char fullFileName[1024];
    snprintf(fullFileName, 1024, "%s/%s.size", file_directory, filename_prefix);

    outputFile = fopen(fullFileName, "w");
    fprintf(outputFile, "%u", (unsigned int) size);
    fclose(outputFile);
}

static AVStream *add_output_stream(AVFormatContext *output_format_context, AVStream *input_stream) {
    AVCodecContext *input_codec_context;
    AVCodecContext *output_codec_context;
    AVStream *output_stream;

    output_stream = avformat_new_stream(output_format_context, 0);
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
#ifdef USE_OLD_FFMPEG
        case CODEC_TYPE_AUDIO:
#else
        case AVMEDIA_TYPE_AUDIO:
#endif
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
#ifdef USE_OLD_FFMPEG
        case CODEC_TYPE_VIDEO:
#else
        case AVMEDIA_TYPE_VIDEO:
#endif
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

int write_index_file(
		const char *index, const char *tmp_index, 
		unsigned int planned_segment_duration,unsigned int numsegments, unsigned int *actual_segment_duration, unsigned int segment_number_offset, 
        const char *output_prefix, const char *output_file_extension,int islast
) {
	if (numsegments<1) return 0;
	FILE *tmp_index_fp;

	unsigned int i;

	tmp_index_fp = fopen(tmp_index, "w");
	if (!tmp_index_fp) {
        fprintf(stderr, "Could not open temporary m3u8 index file (%s), no index file will be created\n", tmp_index);
        return -1;
    }


    unsigned int maxDuration = actual_segment_duration[0];

    for (i = 1; i <numsegments; i++) if (actual_segment_duration[i] > maxDuration) maxDuration = actual_segment_duration[i];



	fprintf(tmp_index_fp,  "#EXTM3U\n#EXT-X-TARGETDURATION:%u\n", maxDuration);
	

	for (i = 0; i <numsegments; i++) {
		if (fprintf(tmp_index_fp, "#EXTINF:%u,\n%s-%u%s\n", actual_segment_duration[i], output_prefix, i+segment_number_offset, output_file_extension)<0){
			fprintf(stderr, "Failed to write to tmp m3u8 index file\n");
			return -1;
		}
    }

    if (islast) {
		if (fprintf(tmp_index_fp, "#EXT-X-ENDLIST\n")<0){
			fprintf(stderr, "Failed to write to tmp m3u8 index file\n");
			return -1;
		};
	}
	fclose(tmp_index_fp);

    return rename(tmp_index, index);
}

int main(int argc, const char *argv[]) {
    //input parameters
	FNHOLDER(inputFilename);
	FNHOLDER(playlistFilename);
	FNHOLDER(baseDirName);
	FNHOLDER(baseFileName);

	
	char baseFileExtension[MAXT_EXT_LENGTH+1];baseFileExtension[MAXT_EXT_LENGTH]=0; //either "ts", "aac" or "mp3"
    int segmentLength, outputStreams, verbosity, version,usage,doid3;



	FNHOLDER(currentOutputFileName);
	FNHOLDER(tempPlaylistName);


    //these are used to determine the exact length of the current segment
    double prev_segment_time = 0;
    double segment_time;
    unsigned int actual_segment_durations[2048];
    double packet_time = 0;

    //new variables to keep track of output size
    double output_bytes = 0;

    unsigned int output_index = 1;
    AVOutputFormat *ofmt;
    AVFormatContext *ic = NULL;
    AVFormatContext *oc;
    AVStream *video_st = NULL;
    AVStream *audio_st = NULL;
    AVCodec *codec;
    int video_index;
    int audio_index;
    unsigned int first_segment = 1;
    unsigned int last_segment = 0;
    int write_index = 1;
    int decode_done;
    int ret;
    int i;

    unsigned char id3_tag[128];
    unsigned char * image_id3_tag;

    size_t id3_tag_size = 73;
    int newFile = 1; //a boolean value to flag when a new file needs id3 tag info in it

    if (parseCommandLine(inputFilename, playlistFilename, baseDirName, baseFileName, baseFileExtension, &outputStreams, &segmentLength, &verbosity, &version,&usage, &doid3,  argc, argv) != 0)
        return 0;

	if (usage) printUsage();
    if (version) ffmpeg_version();
	if (version || usage) return 0;

	//fprintf(stderr, "Options parsed: inputFilename:%s playlistFilename:%s baseDirName:%s baseFileName:%s baseFileExtension:%s segmentLength:%d\n",inputFilename,playlistFilename,baseDirName,baseFileName,baseFileExtension,segmentLength );
	
	
	snprintf(tempPlaylistName, MAX_FILENAME_LENGTH, "%s/%s", baseDirName, playlistFilename);
	strncpy(playlistFilename, tempPlaylistName, MAX_FILENAME_LENGTH);
	snprintf(tempPlaylistName, MAX_FILENAME_LENGTH, "%s.tmp", playlistFilename);
	
    fprintf(stderr, "pl:%s tpl:%s\n", playlistFilename, tempPlaylistName);

	if (doid3) {
		image_id3_tag = malloc(IMAGE_ID3_SIZE);
		if (outputStreams == OUTPUT_STREAM_AUDIO) build_image_id3_tag(image_id3_tag,NULL);
		build_id3_tag((char *) id3_tag, id3_tag_size);
	}


    //decide if this is an aac file or a mpegts file.
    //postpone deciding format until later
    /*	ifmt = av_find_input_format("mpegts");
    if (!ifmt) 
    {
    fprintf(stderr, "Could not find MPEG-TS demuxer.\n");
    exit(1);
    } */

    av_log_set_level(AV_LOG_DEBUG);

    av_register_all();
    ret = avformat_open_input(&ic, inputFilename, NULL, NULL);
    if (ret != 0) {
        fprintf(stderr, "Could not open input file %s. Error %d.\n", inputFilename, ret);
        exit(1);
    }

    if (avformat_find_stream_info(ic, NULL) < 0) {
        fprintf(stderr, "Could not read stream information.\n");
        exit(1);
    }

    oc = avformat_alloc_context();
    if (!oc) {
        fprintf(stderr, "Could not allocate output context.");
        exit(1);
    }

    video_index = -1;
    audio_index = -1;

    for (i = 0; i < ic->nb_streams && (video_index < 0 || audio_index < 0); i++) {
        switch (ic->streams[i]->codec->codec_type) {
#ifdef USE_OLD_FFMPEG
            case CODEC_TYPE_VIDEO:
#else
            case AVMEDIA_TYPE_VIDEO:
#endif
                video_index = i;
                ic->streams[i]->discard = AVDISCARD_NONE;
                if (outputStreams & OUTPUT_STREAM_VIDEO)
                    video_st = add_output_stream(oc, ic->streams[i]);
                break;
#ifdef USE_OLD_FFMPEG
            case CODEC_TYPE_AUDIO:
#else
            case AVMEDIA_TYPE_AUDIO:
#endif
                audio_index = i;
                ic->streams[i]->discard = AVDISCARD_NONE;
                if (outputStreams & OUTPUT_STREAM_AUDIO)
                    audio_st = add_output_stream(oc, ic->streams[i]);
                break;
            default:
                ic->streams[i]->discard = AVDISCARD_ALL;
                break;
        }
    }

    if (video_index == -1) {
        fprintf(stderr, "Source stream must have video component.\n");
        exit(1);
    }

    //now that we know the audio and video output streams
    //we can decide on an output format.
    if (outputStreams == OUTPUT_STREAM_AUDIO) {
        //the audio output format should be the same as the audio input format
        switch (ic->streams[audio_index]->codec->codec_id) {
            case CODEC_ID_MP3:
                fprintf(stderr, "Setting output audio to mp3.");
				strncpy(baseFileExtension, ".mp3", MAXT_EXT_LENGTH);
                ofmt = av_guess_format("mp3", NULL, NULL);
                break;
            case CODEC_ID_AAC:
                fprintf(stderr, "Setting output audio to aac.");
                ofmt = av_guess_format("adts", NULL, NULL);
                break;
            default:
                fprintf(stderr, "Codec id %d not supported.\n", ic->streams[audio_index]->id);
        }
        if (!ofmt) {
            fprintf(stderr, "Could not find audio muxer.\n");
            exit(1);
        }
    } else {
        ofmt = av_guess_format("mpegts", NULL, NULL);
        if (!ofmt) {
            fprintf(stderr, "Could not find MPEG-TS muxer.\n");
            exit(1);
        }
    }
    oc->oformat = ofmt;

    if (outputStreams & OUTPUT_STREAM_VIDEO && oc->oformat->flags & AVFMT_GLOBALHEADER) {
        oc->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    /*  pass the options to avformat_write_header directly. 
        if (av_set_parameters(oc, NULL) < 0) {
            fprintf(stderr, "Invalid output format parameters.\n");
            exit(1);
        }
     */

    av_dump_format(oc, 0, baseFileName, 1);


    //open the video codec only if there is video data
    if (video_index != -1) {
        if (outputStreams & OUTPUT_STREAM_VIDEO)
            codec = avcodec_find_decoder(video_st->codec->codec_id);
        else
            codec = avcodec_find_decoder(ic->streams[video_index]->codec->codec_id);
        if (!codec) {
            fprintf(stderr, "Could not find video decoder, key frames will not be honored.\n");
        }

        if (outputStreams & OUTPUT_STREAM_VIDEO)
            ret = avcodec_open2(video_st->codec, codec, NULL);
        else
            avcodec_open2(ic->streams[video_index]->codec, codec, NULL);
        if (ret < 0) {
            fprintf(stderr, "Could not open video decoder, key frames will not be honored.\n");
        }
    }

    snprintf(currentOutputFileName, MAX_FILENAME_LENGTH, "%s/%s-%u%s", baseDirName, baseFileName, output_index++, baseFileExtension);

    if (avio_open(&oc->pb, currentOutputFileName,AVIO_FLAG_WRITE) < 0) {
        fprintf(stderr, "Could not open '%s'.\n", currentOutputFileName);
        exit(1);
    }
    newFile = 1;

    int r = avformat_write_header(oc, NULL);
    if (r) {
        fprintf(stderr, "Could not write mpegts header to first output file.\n");
        debugReturnCode(r);
        exit(1);
    }

    //no segment info is written here. This just creates the shell of the playlist file
    write_index = !write_index_file(playlistFilename, tempPlaylistName, segmentLength, actual_segment_durations, baseDirName, baseFileName, baseFileExtension, first_segment, last_segment);

    do {
        AVPacket packet;

        decode_done = av_read_frame(ic, &packet);

        if (decode_done < 0) {
            break;
        }

        if (av_dup_packet(&packet) < 0) {
            fprintf(stderr, "Could not duplicate packet.");
            av_free_packet(&packet);
            break;
        }

        //this time is used to check for a break in the segments
        //	if (packet.stream_index == video_index && (packet.flags & PKT_FLAG_KEY)) 
        //	{
        //    segment_time = (double)video_st->pts.val * video_st->time_base.num / video_st->time_base.den;			
        //	}
#if USE_OLD_FFMPEG
        if (packet.stream_index == video_index && (packet.flags & PKT_FLAG_KEY))
#else
        if (packet.stream_index == video_index && (packet.flags & AV_PKT_FLAG_KEY))
#endif		
        {
            segment_time = (double) packet.pts * ic->streams[video_index]->time_base.num / ic->streams[video_index]->time_base.den;
        }
        //  else if (video_index < 0) 
        //	{
        //		segment_time = (double)audio_st->pts.val * audio_st->time_base.num / audio_st->time_base.den;
        //	}

        //get the most recent packet time
        //this time is used when the time for the final segment is printed. It may not be on the edge of
        //of a keyframe!
        if (packet.stream_index == video_index)
            packet_time = (double) packet.pts * ic->streams[video_index]->time_base.num / ic->streams[video_index]->time_base.den; //(double)video_st->pts.val * video_st->time_base.num / video_st->time_base.den;
        else if (outputStreams & OUTPUT_STREAM_AUDIO)
            packet_time = (double) audio_st->pts.val * audio_st->time_base.num / audio_st->time_base.den;
        else
            continue;
        //start looking for segment splits for videos one half second before segment duration expires. This is because the 
        //segments are split on key frames so we cannot expect all segments to be split exactly equally. 
        if (segment_time - prev_segment_time >= segmentLength - 0.5) {
            fprintf(stderr, "looking to print index file at time %lf\n", segment_time);
            avio_flush(oc->pb);
            avio_close(oc->pb);

            if (write_index) {
                actual_segment_durations[++last_segment] = (unsigned int) rint(segment_time - prev_segment_time);
                write_index = !write_index_file(playlistFilename, tempPlaylistName, segmentLength, actual_segment_durations, baseDirName, baseFileName, baseFileExtension, first_segment, last_segment);
                fprintf(stderr, "Writing index file at time %lf\n", packet_time);
            }

            struct stat st;
            stat(currentOutputFileName, &st);
            output_bytes += st.st_size;

            snprintf(currentOutputFileName, MAX_FILENAME_LENGTH, "%s/%s-%u%s", baseDirName, baseFileName, output_index++, baseFileExtension);
            if (avio_open(&oc->pb, currentOutputFileName, AVIO_FLAG_WRITE) < 0) {
                fprintf(stderr, "Could not open '%s'\n", currentOutputFileName);
                break;
            }

            newFile = 1;
            prev_segment_time = segment_time;
        }

        if (outputStreams == OUTPUT_STREAM_AUDIO && packet.stream_index == audio_index) {
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

            packet.stream_index = 0; //only one stream in audio only segments
            ret = av_interleaved_write_frame(oc, &packet);
        } else if (outputStreams & OUTPUT_STREAM_VIDEO) {
            if (newFile) {
                //fprintf(stderr, "New File: %lld %lld %lld\n", packet.pts, video_st->pts.val, audio_st->pts.val);
                //printf("%lf %lld %lld %lld %lld %lld %lf\n", segment_time, audio_st->pts.val, audio_st->cur_dts, audio_st->cur_pkt.pts, packet.pts, packet.dts, packet.dts * av_q2d(ic->streams[audio_index]->time_base) );
                newFile = 0;
            }
            if (outputStreams == OUTPUT_STREAM_VIDEO)
                ret = av_write_frame(oc, &packet);
            else
                ret = av_interleaved_write_frame(oc, &packet);
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
    avio_flush(oc->pb);
    av_write_trailer(oc);

    if (video_st && (video_st->codec->codec !=NULL))
        avcodec_close(video_st->codec);

    if (audio_st && (audio_st->codec->codec != NULL)){
        avcodec_close(audio_st->codec);
    }

    for (i = 0; i < oc->nb_streams; i++) {
        av_freep(&oc->streams[i]->codec);
        av_freep(&oc->streams[i]);
    }

    avio_close(oc->pb);
    av_free(oc);

    struct stat st;
    stat(currentOutputFileName, &st);
    output_bytes += st.st_size;


    if (write_index) {
        actual_segment_durations[++last_segment] = (unsigned int) rint(packet_time - prev_segment_time);

        //make sure that the last segment length is not zero
        if (actual_segment_durations[last_segment] == 0)
            actual_segment_durations[last_segment] = 1;

        write_index_file(playlistFilename, tempPlaylistName, segmentLength, actual_segment_durations, baseDirName, baseFileName, baseFileExtension, first_segment, last_segment);

    }

    write_stream_size_file(baseDirName, baseFileName, output_bytes * 8 / segment_time);

    return 0;
}
