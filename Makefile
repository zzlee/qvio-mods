SUBDIRS = \
	modules/qdmabuf \
	modules/qvio-l4t

SUBDIRS += \
	samples/01_v4l-ctl \
	samples/02_qdmabuf-ctl \
	samples/03_qvio-ctl \
	samples/04_qviod

.PHONY: all
all:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
		echo "Make in $$subdir";\
		$(MAKE) -C $$subdir;\
		if [ $$? -ne 0 ]; then exit 1; fi;\
	done

.PHONY: clean
clean:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
		echo "Clean in $$subdir";\
		$(MAKE) -C $$subdir clean;\
	done