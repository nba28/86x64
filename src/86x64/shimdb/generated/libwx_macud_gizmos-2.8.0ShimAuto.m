#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

static void shim_note(const char *sym) {
  static const char *seen[2048]; static int n;
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mtx);
  for (int i = 0; i < n; i++)
    if (seen[i] == sym) { pthread_mutex_unlock(&mtx); return; }
  if (n < 2048) seen[n++] = sym;
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libwx_macud_gizmos-2.8.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long ZN15wxLEDNumberCtrl12SetAlignmentE15wxLEDValueAlignb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxLEDNumberCtrl12SetAlignmentE15wxLEDValueAlignb"); return 0; }

long ZN15wxLEDNumberCtrl12SetDrawFadedEbb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxLEDNumberCtrl12SetDrawFadedEbb"); return 0; }

long ZN15wxLEDNumberCtrl6CreateEP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxLEDNumberCtrl6CreateEP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN15wxLEDNumberCtrl8SetValueERK8wxStringb(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxLEDNumberCtrl8SetValueERK8wxStringb"); return 0; }

long ZN15wxLEDNumberCtrlC1EP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxLEDNumberCtrlC1EP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN15wxLEDNumberCtrlC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxLEDNumberCtrlC1Ev"); return 0; }

long ZN15wxStaticPicture6CreateEP8wxWindowiRK8wxBitmapRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxStaticPicture6CreateEP8wxWindowiRK8wxBitmapRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN15wxStaticPicture9SetBitmapERK8wxBitmap(long a, long b, long c_, long d, long e, long f) { shim_note("ZN15wxStaticPicture9SetBitmapERK8wxBitmap"); return 0; }

long ZN17wxEditableListBox10GetStringsER13wxArrayString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxEditableListBox10GetStringsER13wxArrayString"); return 0; }

long ZN17wxEditableListBox10SetStringsERK13wxArrayString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxEditableListBox10SetStringsERK13wxArrayString"); return 0; }

long ZN17wxEditableListBoxC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN17wxEditableListBoxC1EP8wxWindowiRK8wxStringRK7wxPointRK6wxSizelS4_"); return 0; }

long ZN19wxDynamicSashWindowC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxDynamicSashWindowC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxString"); return 0; }

long ZN19wxDynamicSashWindowC1Ev(long a, long b, long c_, long d, long e, long f) { shim_note("ZN19wxDynamicSashWindowC1Ev"); return 0; }

long ZN20wxThinSplitterWindowC1EP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN20wxThinSplitterWindowC1EP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN21wxTreeCompanionWindow8DrawItemER4wxDC12wxTreeItemIdRK6wxRect(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxTreeCompanionWindow8DrawItemER4wxDC12wxTreeItemIdRK6wxRect"); return 0; }

long ZN21wxTreeCompanionWindowC2EP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN21wxTreeCompanionWindowC2EP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN23wxDynamicSashSplitEventC1EP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxDynamicSashSplitEventC1EP8wxObject"); return 0; }

long ZN23wxDynamicSashUnifyEventC1EP8wxObject(long a, long b, long c_, long d, long e, long f) { shim_note("ZN23wxDynamicSashUnifyEventC1EP8wxObject"); return 0; }

long ZN24wxSplitterScrolledWindowC1EP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN24wxSplitterScrolledWindowC1EP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZN26wxRemotelyScrolledTreeCtrl12ScrollToLineEii(long a, long b, long c_, long d, long e, long f) { shim_note("ZN26wxRemotelyScrolledTreeCtrl12ScrollToLineEii"); return 0; }

long ZN26wxRemotelyScrolledTreeCtrl14HideVScrollbarEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN26wxRemotelyScrolledTreeCtrl14HideVScrollbarEv"); return 0; }

long ZN26wxRemotelyScrolledTreeCtrl22AdjustRemoteScrollbarsEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN26wxRemotelyScrolledTreeCtrl22AdjustRemoteScrollbarsEv"); return 0; }

long ZN26wxRemotelyScrolledTreeCtrlC1EP8wxWindowiRK7wxPointRK6wxSizel(long a, long b, long c_, long d, long e, long f) { shim_note("ZN26wxRemotelyScrolledTreeCtrlC1EP8wxWindowiRK7wxPointRK6wxSizel"); return 0; }

long ZNK21wxTreeCompanionWindow12GetClassInfoEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK21wxTreeCompanionWindow12GetClassInfoEv"); return 0; }

long ZNK21wxTreeCompanionWindow13GetEventTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK21wxTreeCompanionWindow13GetEventTableEv"); return 0; }

long ZNK21wxTreeCompanionWindow17GetEventHashTableEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK21wxTreeCompanionWindow17GetEventHashTableEv"); return 0; }

long ZNK26wxRemotelyScrolledTreeCtrl17GetScrolledWindowEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZNK26wxRemotelyScrolledTreeCtrl17GetScrolledWindowEv"); return 0; }
