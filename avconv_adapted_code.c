/*
 * Copyright (c) 2000-2011 The libav developers.
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
 * This code is adapted from the original avconv.c from libav project
 **********************************************************************************/

/**********************************************************************************
 * This code is part of HTTP-Live-Video-Stream-Segmenter-and-Distributor
 * look for newer versions at github.com
 **********************************************************************************/

#include "segmenter.h"
#include <stdio.h>
/*
 

typedef struct InputStream {
    int file_index;
    AVStream *st;
    int discard;             / * true if stream data should be discarded * /
    int decoding_needed;     / * true if the packets must be decoded in 'raw_fifo' * /
    AVCodec *dec;
    AVFrame *decoded_frame;
    AVFrame *filtered_frame;

    int64_t       start;     / * time when read started * /
    / * predicted dts of the next packet read for this stream or (when there are
     * several frames in a packet) of the next frame in current packet * /
    int64_t       next_dts;
    / * dts of the last packet read for this stream * /
    int64_t       last_dts;
    PtsCorrectionContext pts_ctx;
    double ts_scale;
    int is_start;            / * is 1 at the start and after a discontinuity * /
    int showed_multi_packet_warning;
    AVDictionary *opts;

    / * a pool of free buffers for decoded data * /
    FrameBuffer *buffer_pool;
} InputStream;

typedef struct InputFile {
    AVFormatContext *ctx;
    int eof_reached;      / * true if eof reached * /
    int ist_index;        / * index of first stream in ist_table * /
    int buffer_size;      / * current total buffer size * /
    int64_t ts_offset;
    int nb_streams;       / * number of stream that avconv is aware of; may be different
                             from ctx.nb_streams if new streams appear during av_read_frame() * /
    int rate_emu;
} InputFile;

typedef struct OutputStream {
    int file_index;          / * file index * /
    int index;               / * stream index in the output file * /
    int source_index;        / * InputStream index * /
    AVStream *st;            / * stream in the output file * /
    int encoding_needed;     / * true if encoding needed for this stream * /
    int frame_number;
    / * input pts and corresponding output pts
       for A/V sync * /
    // double sync_ipts;        / * dts from the AVPacket of the demuxer in second units * /
    struct InputStream *sync_ist; / * input stream to sync against * /
    int64_t sync_opts;       / * output frame counter, could be changed to some true timestamp * / // FIXME look at frame_number
    / * pts of the first frame encoded for this stream, used for limiting
     * recording time * /
    int64_t first_pts;
    AVBitStreamFilterContext *bitstream_filters;
    AVCodec *enc;
    int64_t max_frames;
    AVFrame *output_frame;

    / * video only * /
    int video_resample;
    AVFrame pict_tmp;      / * temporary image for resampling * /
    struct SwsContext *img_resample_ctx; / * for image resampling * /
    int resample_height;
    int resample_width;
    int resample_pix_fmt;
    AVRational frame_rate;
    int force_fps;
    int top_field_first;

    float frame_aspect_ratio;

    / * forced key frames * /
    int64_t *forced_kf_pts;
    int forced_kf_count;
    int forced_kf_index;

    / * audio only * /
    int audio_resample;
    ReSampleContext *resample; / * for audio resampling * /
    int resample_sample_fmt;
    int resample_channels;
    int resample_sample_rate;
    int reformat_pair;
    AVAudioConvert *reformat_ctx;
    AVFifoBuffer *fifo;     / * for compression: one audio fifo per codec * /
    FILE *logfile;

#if CONFIG_AVFILTER
    AVFilterContext *output_video_filter;
    AVFilterContext *input_video_filter;
    AVFilterBufferRef *picref;
    char *avfilter;
    AVFilterGraph *graph;
#endif

    int64_t sws_flags;
    AVDictionary *opts;
    int is_past_recording_time;
    int stream_copy;
    const char *attachment_filename;
    int copy_initial_nonkeyframes;
} OutputStream;


typedef struct OutputFile {
    AVFormatContext *ctx;
    AVDictionary *opts;
    int ost_index;       / * index of the first stream in output_streams * /
    int64_t recording_time; / * desired length of the resulting file in microseconds * /
    int64_t start_time;     / * start time in microseconds * /
    uint64_t limit_filesize;
} OutputFile;

*/
 


void do_streamcopy(AVStream *i_st, AVStream *o_st, AVFormatContext *o_fctx, const AVPacket *pkt, do_streamcopy_opts* opts)
{
    //OutputFile *of = &output_files[ost->file_index];
    //int64_t ost_tb_start_time = av_rescale_q(of->start_time, AV_TIME_BASE_Q, ost->st->time_base);

    if (opts->skiptokeyframe  && !(pkt->flags & AV_PKT_FLAG_KEY)) return;
	opts->skiptokeyframe=0;

    AVPacket opkt;
    av_init_packet(&opkt);

    if (pkt->pts != AV_NOPTS_VALUE) {
        opkt.pts = av_rescale_q(pkt->pts, i_st->time_base, o_st->time_base) - opts->ost_tb_start_time;
		opts->last_pts=opkt.pts;
	} else
        opkt.pts = AV_NOPTS_VALUE;

    if (pkt->dts == AV_NOPTS_VALUE) {
        opkt.dts =opts->last_dts;
	} else {
        opkt.dts = av_rescale_q(pkt->dts, i_st->time_base, o_st->time_base);
		opts->last_dts=opkt.dts;
	}
    opkt.dts -= opts->ost_tb_start_time;

    opkt.duration = av_rescale_q(pkt->duration, i_st->time_base, o_st->time_base);
    opkt.flags    = pkt->flags;

    // FIXME remove the following 2 lines they shall be replaced by the bitstream filters
    if (  o_st->codec->codec_id != CODEC_ID_H264
       && o_st->codec->codec_id != CODEC_ID_MPEG1VIDEO
       && o_st->codec->codec_id != CODEC_ID_MPEG2VIDEO
       ) {
        if (av_parser_change(i_st->parser, o_st->codec, &opkt.data, &opkt.size, pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY))
            opkt.destruct = av_destruct_packet;
    } else {
        opkt.data = pkt->data;
        opkt.size = pkt->size;
    }

    write_frame(o_fctx, &opkt, o_st, opts);
    o_st->codec->frame_number++;
    av_free_packet(&opkt);
}


 void write_frame(AVFormatContext *s, AVPacket *pkt, AVStream *o_st, do_streamcopy_opts* opts)
{
    AVBitStreamFilterContext *bsfc = opts->bitstream_filters;
    AVCodecContext          *avctx = o_st->codec;
    int ret;

    /*
     * Audio encoders may split the packets --  #frames in != #packets out.
     * But there is no reordering, so we can limit the number of output packets
     * by simply dropping them here.
     * Counting encoded video frames needs to be done separately because of
     * reordering, see do_video_out()
     */


    while (bsfc) {
        AVPacket new_pkt = *pkt;
        int a = av_bitstream_filter_filter(bsfc, avctx, NULL,
                                           &new_pkt.data, &new_pkt.size,
                                           pkt->data, pkt->size,
                                           pkt->flags & AV_PKT_FLAG_KEY);
        if (a > 0) {
            av_free_packet(pkt);
            new_pkt.destruct = av_destruct_packet;
        } else if (a < 0) {
            av_log(NULL, AV_LOG_ERROR, "%s failed for stream %d, codec %s",
                   bsfc->filter->name, pkt->stream_index,
                   avctx->codec ? avctx->codec->name : "copy");
			
            //print_error("", a);
            //if (exit_on_error) exit_program(1);
        }
        *pkt = new_pkt;

        bsfc = bsfc->next;
    }

    pkt->stream_index = o_st->index;
	
	if (opts->interleave_write) {
		ret = av_interleaved_write_frame(s, pkt);
	} else {
		ret = av_write_frame(s, pkt);
	}
	
    if (ret < 0) {
        fprintf (stderr,"ERROR: av_interleaved_write_frame() got error: %d", ret);
        //exit_program(1);
    }
}