#pragma once

#include "StiebelEltronProtocolImpl.h"
#include "ApplicationContainer.h"
#include "SerialCan.h"

using StiebelEltronProtocol = impl::StiebelEltronProtocol<SerialCan>;
