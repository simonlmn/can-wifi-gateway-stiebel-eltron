#ifndef OPERATIONRESULT_H_
#define OPERATIONRESULT_H_

#include <toolbox/String.h>

// Describes the outcome of a queued/send-like operation.
enum struct OperationResult {
	Accepted,     // Operation queued/accepted for processing
	RateLimited,  // Rejected because a rate limit/budget prevented acceptance
	QueueFull,    // Rejected because the queue was full
	NotReady,     // Rejected because the subsystem was not ready/initialized
	Unavailable,  // Rejected because the operation is not available in the current mode/state
	Invalid,      // Rejected because parameters were invalid
};

toolbox::strref operationResultToString(OperationResult result) {
	switch (result) {
		case OperationResult::Accepted: return F("Accepted");
		case OperationResult::RateLimited: return F("RateLimited");
		case OperationResult::QueueFull: return F("QueueFull");
		case OperationResult::NotReady: return F("NotReady");
		case OperationResult::Unavailable: return F("Unavailable");
		case OperationResult::Invalid: return F("Invalid");
		default: return F("?OperationResult?");
	}
}

#endif
