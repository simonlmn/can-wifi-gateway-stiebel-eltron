#ifndef OPERATIONRESULT_H_
#define OPERATIONRESULT_H_

// Describes the outcome of a queued/send-like operation.
enum struct OperationResult {
	Accepted,     // Operation queued/accepted for processing
	RateLimited,  // Rejected because a rate limit/budget prevented acceptance
	QueueFull,    // Rejected because the queue was full
	NotReady,     // Rejected because the subsystem was not ready/initialized
	Invalid,      // Rejected because parameters were invalid
};

const char* operationResultToString(OperationResult result) {
	switch (result) {
		case OperationResult::Accepted: return "Accepted";
		case OperationResult::RateLimited: return "RateLimited";
		case OperationResult::QueueFull: return "QueueFull";
		case OperationResult::NotReady: return "NotReady";
		case OperationResult::Invalid: return "Invalid";
		default: return "?OperationResult?";
	}
}

#endif
