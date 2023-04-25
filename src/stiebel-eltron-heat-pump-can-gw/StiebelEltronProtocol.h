#pragma once

#include "StiebelEltronProtocolImpl.h"
#include "NodeBase.h"
#include "SerialCan.h"

using StiebelEltronProtocol = impl::StiebelEltronProtocol<NodeBase, SerialCan>;
