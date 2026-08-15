#ifndef SWI_DEFINES_H
#define SWI_DEFINES_H

/* =============================================================================
 * Defines
 * ============================================================================= */

#include <math.h>
#include <stdint.h>
#include <stddef.h>

/**
* Library Defines. Change as needed.
*/

// Max Input Signal Size.
#ifndef SWE_MAX_SAMPLES
#define SWE_MAX_SAMPLES 1024
#endif

// Max Decompsitioned Components size.
#ifndef SWE_MAX_COMPONENTS
#define SWE_MAX_COMPONENTS 200
#endif

#ifndef SWI_DEFINED_PI
#define SWI_PI       3.14159265358979323846
#define SWI_TWO_PI   6.28318530717958647692
#define SWI_FOUR_PI 12.56637061435917295384
#define SWI_SIX_PI  18.84955592153875943076
#endif

/**
* Math.h dependent defines. You may swap them out.
*/
#ifndef SWI_DEFINED_SQRT
#define swi_sqrt(x) sqrt(x)
#endif

#endif /* SWI_DEFINES_H */
