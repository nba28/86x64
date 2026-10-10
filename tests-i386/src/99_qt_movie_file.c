/* 99_qt_movie_file — a movie opened the classic file way (FSSpec ->
 * OpenMovieFile -> NewMovieFromFile) lands in the AVFoundation Movie bridge.
 *
 * Halo plays its studio logos this way. The open used to fall through to the
 * translated QuickTime.framework, so the bridge never saw the Movie and the
 * intros were skipped. The harness writes a short H.264 movie and passes its
 * path in QTMF_PATH.
 *
 * ON exits 42: a Movie with a duration, closed file refNum, loop flag set
 * through its TimeBase. OFF (M64_NO_QT_MOVIE_FILE=1, run time): OpenMovieFile
 * declines -> exit 2.
 */
extern void exit(int);
extern int  printf(const char *, ...);
extern char *getenv(const char *);

typedef short OSErr;
typedef void *Movie;
OSErr FSPathMakeRef(const unsigned char *path, void *ref, unsigned char *isDir);
OSErr FSGetCatalogInfo(const void *ref, unsigned int which, void *catInfo,
                       void *outName, void *fsSpec, void *parentRef);
OSErr EnterMovies(void);
OSErr OpenMovieFile(const void *spec, short *resRefNum, signed char perm);
OSErr NewMovieFromFile(Movie *m, short resRefNum, short *resId, unsigned char *resName,
                       short flags, unsigned char *dataRefWasChanged);
OSErr CloseMovieFile(short resRefNum);
long  GetMovieDuration(Movie m);
void *GetMovieTimeBase(Movie m);
void  SetTimeBaseFlags(void *tb, long flags);
long  GetTimeBaseFlags(void *tb);
void  DisposeMovie(Movie m);

int main(void) {
   unsigned char ref[80], spec[70];
   const char *path = getenv("QTMF_PATH");
   short rn = -1;
   Movie mv = 0;
   if (!path || FSPathMakeRef((const unsigned char *)path, ref, 0) ||
       FSGetCatalogInfo(ref, 0, 0, 0, spec, 0)) exit(1);
   EnterMovies();
   if (OpenMovieFile(spec, &rn, 1)) exit(2);
   if (NewMovieFromFile(&mv, rn, 0, 0, 1 /*newMovieActive*/, 0) || !mv) exit(3);
   if (CloseMovieFile(rn)) exit(4);
   long dur = GetMovieDuration(mv);
   void *tb = GetMovieTimeBase(mv);
   SetTimeBaseFlags(tb, 1);
   long fl = GetTimeBaseFlags(tb);
   printf("refnum=%d duration=%ld tb=%p flags=%ld\n", rn, dur, tb, fl);
   DisposeMovie(mv);
   exit(dur > 0 && tb && fl == 1 ? 42 : 5);
}
