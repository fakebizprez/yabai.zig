#ifndef EVENT_SERIALIZE_H
#define EVENT_SERIALIZE_H

#include <ApplicationServices/ApplicationServices.h>

/* macOS 27 dock swipes require raw IOHID data; the caller releases the returned event. */
CGEventRef iss_augment_dock_swipe_event(CGEventRef event);

#endif
