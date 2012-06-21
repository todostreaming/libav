#!/bin/bash
for file in ../libavfilter/af_{aformat,anull,asyncts,resample}.c \
            ../libavfilter/avfilter{,graph}.c \
            ../libavfilter/{asink_anullsink.c,asrc_anullsrc.c} \
            ../libavfilter/buffer{src,sink}.c \
            ../libavfilter/vf_{aspect,blackframe,boxblur,copy,crop,cropdetect,delogo,drawbox,fade,fieldorder,fifo,format,gradfun,hflip,hqdn3d,lut,null,overlay,pad,pixdesctest,scale,select,setpts,settb,showinfo,slicify,split,transpose,unsharp,vflip,yadif}.c \
            ../libavfilter/vsink_nullsink.c \
            ../libavfilter/vsrc_{color,movie,nullsrc,testsrc}.c \
            ../libavformat/{4xm,a64,aacdec,ac3dec,adtsenc,adxdec,aea,aiff{dec,enc},amr,anm,apc,ape,asf{dec,enc},ass{dec,enc},au,avi{dec,enc},avio,aviobuf,avs}.c \
			../libavformat/{bethsoftvid,bfi,bink,bmv,c93,cafdec,cdg,cdxl,concat,crcenc,crypto,daud,dfa,dsicin,dtsdec,dv,dvenc,dxa,eacdata,electronicarts}.c \
			../libavformat/{ffm{dec,enc},ffmeta{dec,enc},file,filmstrip{dec,enc},flac{dec,enc},flic,flv{dec,enc},framecrcenc,gif,gopher,gsmdec,gxf{,enc}}.c \
			../libavformat/{hls,hlsproto,http,id3v1,idcin,idroq{dec,enc},iff,img2{dec,enc},ingenientdec,ipmovie,iss,iv8,ivf{dec,enc},jvdec,latmenc,lmlm4,lxfdec}.c \
			../libavformat/{matroska{dec,enc},md5{enc,proto},mm,mmf,mms{h,t},mov,movenc,mp3{dec,enc},mpc,mpc8,mpeg,mpegenc,mpegts,mpegtsenc,mpjpeg,msnwc_tcp,mtv}.c \
			../libavformat/{mvi,mxf{dec,enc},mxg,ncdec,network,nsvdec,nullenc,nut{dec,enc},nuv,ogg{dec,enc},oggparse{celt,dirac,flac,ogm,skeleton,speex,theora,vorbis}}.c \
			../libavformat/{oma{dec,enc},options,pcm{dec,enc},pmpdec,psxstr,pva,qcp,r3d,raw{dec,enc},rawvideodec,rdt,rl2,rm{dec,enc},rpl,rso{dec,enc}}.c \
			../libavformat/{rtmpproto,rtpdec{,_amr,_asf,_g726,_h263,_h263_rfc2190,_h264,_latm,_mpeg4,_qcelp,_qdm2,_qt,_svq3,_vp8,_xiph},rtpenc,rtpproto,rtsp}.c \
			../libavformat/{rtsp{dec,enc},sap{dec,enc},segafilm,segment,sierravmd,siff,smacker,smjpeg{dec,enc},sol,sox{dec,enc},spdif{dec,enc},srtdec,swf{dec,enc}}.c \
			../libavformat/{tcp,thp,tiertexseq,tmv,tta,tty,txd,udp,utils,vc1test{,enc},voc{dec,enc},vqf,wav,wc3movie,westwood_{aud,vqa},wtv,wv,xa,xmv,xwma,yop,yuv4mpeg}.c \
			../libavformat/seek-test.c \
			../libavformat/{options_table,rawdec}.h \
			../libavresample/{audio_data,options}.c \
			../libavcodec/{4xm,8bps,8svx,a64multienc,aac{_parser,dec,enc,psy},aasc,ac3{_parser,dec,enc{,_opts_template,_fixed,_float}},adpcm{,enc},adx{_parser,dec,enc}}.c \
			../libavcodec/{alac{,enc},alsdec,amrnbdec,amrwbdec,anm,ansi,apedec,ass{dec,enc},asv1,atrac{1,3},audio_frame_queue,aura,avs,bethsoftvideo,bfi,bink{,audio}}.c \
			../libavcodec/{bmp{,enc},bmv,c93,cavs{_parser,dec},cdgraphics,cdxl,cinepak,cljr,cook{,_parser},cscd,cyuv,dca{,_parser},dfa,dirac_parser,dnxhd{_parser,dec,enc}}.c \
			../libavcodec/{dpcm,dpx{,enc},dsicinav,dv{,_profile,dec},dvbsub{,_parser,dec},dvdsub{_parser,dec,enc},dxtory,dxva2_{h264,mpeg2,vc1},eac3enc,ea{cmv,mad,tgq,tgv,tqi}}.c \
			../libavcodec/{escape124,ffv1,flac{_parser,dec,enc},flicvideo,flv{dec,enc},fraps,frwu,g722{dec,enc},g726,gif{,dec},gsm{_parser,dec},h261{_parser,dec,enc}}.c \
			../libavcodec/{h263{_parser,dec},h264{,_parser,_ps},huffyuv,idcinvideo,iff,imc,imgconvert,indeo{2,3,4,5},intelh263dec,interplayvideo,ituh263{dec,enc}}.c \
			../libavcodec/{jpegls{dec,enc},jvdec,kgv1dec,kmvc,lagarith,latm_parser,lcldec,ljpegenc,loco,mace,mdec,mimic,mjpeg{2jpeg_bsf,_parser,bdec,dec,enc},mlp{_parser,dec}}.c \
			../libavcodec/{mmvideo,motionpixels,mpc{7,8},mpeg12{,enc},mpeg4video{_parser,dec,enc},mpegaudio{_parser,dec,dec_float,enc},mpegvideo{_enc,_parser},msmpeg4}.c \
			../libavcodec/{msrle,msvideo1,mxpegdec,nellymoser{dec,enc},nuv,options,pamenc,pcm-mpeg,pcm,pcx{,enc},pgssubdec,pictordec,pnm{_parser,dec,enc},prores{dec,enc}}.c \
			../libavcodec/{ptx,qcelpdec,qdm2,qdrw,qpeg,qtrle{,enc},r210dec,ra144{dec,enc},ra288,ralf,raw{dec,enc},rl2,roqaudioenc,roqvideo{dec,enc},rpza,rv10{,enc},rv20enc}.c \
			../libavcodec/{rv30,rv34_parser,rv40,s302m,sgi{dec,enc},shorten,sipr,smacker,smc,snow{enc,dec},sp5xdec,srtdec,sunrast{,enc},svq1{dec,enc},svq3,targa{,enc}}.c \
			../libavcodec/{tiertexsegv,tiff{,enc},tmv,truemotion{1,2},truespeech,tta,twinvq,txd,ulti,utils,utvideo,v210{dec,enc,x},v410{dec,enc},vb,vble,vc1{,_parser,dec}}.c \
			../libavcodec/{vcr1,vmdav,vmnc,vorbis{_parser,dec,enc},vp{3{,_parser},5,56,6,8{,_parser}},vqavideo,wavpack,wma{{,lossless,pro}dec,enc,voice},wmv2{dec,enc}}.c \
			../libavcodec/{wnv1,ws-snd1,xan,xbmenc,xl,xsub{dec,enc},xwd{dec,enc},xxan,yop}.c \
			../libavcodec/{internal,mpegvideo,options_table,snow,twinvq_data,vorbis_enc_data,vp8data}.h \
			../libswscale/{options,swscale_unscaled,utils}.c \
			../libavutil/{aes,audioconvert,cpu,crc,des,eval,opt,pixdesc,rational,samplefmt}.c \
			../{avconv,avprobe}.c; do
    ./convert.sh $file 1
done
