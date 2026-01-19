#ifndef OPERATIONRESULT_H_
#define OPERATIONRESULT_H_

// Describes the outcome of a queued/send-like operation.
enum struct OperationResult {
	Accepted,     // Operation queued/accepted for processing
	RateLimited,  // Rejected because a rate limit/budget prevented acceptance
	NotReady,     // Rejected because the subsystem was not ready/initialized
	Invalid,      // Rejected because parameters were invalid
};

#endif
