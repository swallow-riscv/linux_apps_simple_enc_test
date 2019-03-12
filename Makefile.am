AM_CFLAGS =

AM_CPPFLAGS = \
	$(WARN_CFLAGS)	\
	-I.				\
	-I${includedir}

simple_enc_test_LDADD =	\
	-lnx_video_api	\
	-lnx_v4l2

simple_enc_test_SOURCES = \
	main.cpp

bin_PROGRAMS = simple_enc_test
