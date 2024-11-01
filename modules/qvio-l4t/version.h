#ifndef __QVIO_VERSION_H__
#define __QVIO_VERSION_H__

#define DRV_MOD_MAJOR		2024
#define DRV_MOD_MINOR		6
#define DRV_MOD_PATCHLEVEL	0

#define DRV_MODULE_VERSION      \
	__stringify(DRV_MOD_MAJOR) "." \
	__stringify(DRV_MOD_MINOR) "." \
	__stringify(DRV_MOD_PATCHLEVEL)

#define DRV_MOD_VERSION_NUMBER  \
	((DRV_MOD_MAJOR)*1000 + (DRV_MOD_MINOR)*100 + DRV_MOD_PATCHLEVEL)

#endif // __QVIO_VERSION_H__
