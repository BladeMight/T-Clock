#ifndef AUTOVERSION_H
#define AUTOVERSION_H
/* Note: to use integer defines as strings, use STR(), eg. STR(VER_REVISION) */
/**** Version ****/
#	define VER_MAJOR 2
#	define VER_MINOR 4
#	define VER_BUILD 3
	/** status values: 0=Alpha, 1=Beta, 2=RC, 3=Release, 4=Maintenance */
#	define VER_STATUS 1
#	define VER_STATUS_FULL "Beta"
#	define VER_STATUS_SHORT "b"
#	define VER_STATUS_GREEK "β"
#	define VER_REVISION 474
#	define VER_FULL "2.4.3 Beta"
#	define VER_SHORT "2.4b3"
#	define VER_SHORT_DOTS "2.4.3"
#	define VER_SHORT_GREEK "2.4β3"
#	define VER_RC_REVISION 2, 4, 3, 474
#	define VER_RC_STATUS 2, 4, 3, 1
/**** Subversion Information ****/
#	define VER_REVISION_URL "https://github.com/BladeMight/T-Clock"
#	define VER_REVISION_DATE "2017-09-01 11:45:19 +0000 (Fri, Sep 01 2017)"
#	define VER_REVISION_HASH "3609565"
#	define VER_REVISION_TAG "v2.4.3#474-beta"
/**** Date/Time ****/
#	define VER_TIMESTAMP 1508710852
#	define VER_TIME_SEC 52
#	define VER_TIME_MIN 20
#	define VER_TIME_HOUR 22
#	define VER_TIME_DAY 22
#	define VER_TIME_MONTH 10
#	define VER_TIME_YEAR 2017
#	define VER_TIME_WDAY 0
#	define VER_TIME_YDAY 294
#	define VER_TIME_WDAY_SHORT "Sun"
#	define VER_TIME_WDAY_FULL "Sunday"
#	define VER_TIME_MONTH_SHORT "Oct"
#	define VER_TIME_MONTH_FULL "October"
#	define VER_TIME "22:20:52"
#	define VER_DATE "2017-10-22"
#	define VER_DATE_LONG "Sun, Oct 22, 2017 22:20:52 UTC"
#	define VER_DATE_SHORT "2017-10-22 22:20:52 UTC"
#	define VER_DATE_ISO "2017-10-22T22:20:52Z"
/**** Helper 'functions' ****/
#	define VER_IsReleaseOrHigher() ( VER_STATUS >= 3 )
#	define VER_IsAlpha() ( VER_STATUS == 0 )
#	define VER_IsBeta() ( VER_STATUS == 1 )
#	define VER_IsRC() ( VER_STATUS == 2 )
#	define VER_IsRelease() ( VER_STATUS == 3 )
#	define VER_IsMaintenance() ( VER_STATUS == 4 )
#ifndef STR
#	define STR_(x) #x
#	define STR(x) STR_(x)
#endif
#ifndef L
#	define L_(x) L##x
#	define L(x) L_(x)
#endif
#endif
