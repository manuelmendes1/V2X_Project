#pragma once

#include "vanetza_integration/spatem/types/spatem_input.h"

// Initial SPATEM codec uses the same neutral semantic shape for
// application input and decoded message representation.
using SpatemMessage = SpatemInput;
