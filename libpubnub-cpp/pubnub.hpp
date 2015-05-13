#ifndef PUBNUB__PubNub_hpp
#define PUBNUB__PubNub_hpp

#include <string>
#include <vector>

#include <json.h>

#include <pubnub.h>


/** PubNub structures and constants */

struct pubnub;
class PubNub;


/* The PubNub methods return result codes as listed in enum pubnub_res.
 * Please refer to pubnub.h for their listing and brief description. */

/* Callback functions to user code upon completion of various methods. */
/* Note that if the function wants to preserve the response, it should
 * bump its reference count, otherwise it will be auto-released after
 * callback is done. channels[], on the other hand, are dynamically
 * allocated and both the array and its individual items must be free()d
 * by the callee; to ease iteration by user code, there is guaranteed to
 * be as many elements as there are messages in the channels list, and
 * an extra NULL pointer at the end of the array. */
typedef void (*PubNub_publish_cb)(PubNub &p, enum pubnub_res result, json_object *response, void *ctx_data, void *call_data);
typedef void (*PubNub_subscribe_cb)(PubNub &p, enum pubnub_res result, std::vector<std::string> &channels, json_object *response, void *ctx_data, void *call_data);
typedef void (*PubNub_history_cb)(PubNub &p, enum pubnub_res result, json_object *response, void *ctx_data, void *call_data);
typedef void (*PubNub_here_now_cb)(PubNub &p, enum pubnub_res result, json_object *response, void *ctx_data, void *call_data);
typedef void (*PubNub_time_cb)(PubNub &p, enum pubnub_res result, json_object *response, void *ctx_data, void *call_data);


/* Only one method may operate on a single context at once - this means
 * that if a subscribe is in progress, you cannot publish in the same
 * context; either wait or use multiple contexts. If the same context
 * is used in multiple threads, the application must ensure locking to
 * prevent improper concurrent access. */

class PubNub {
public:

	/** PubNub context methods */

	/* Initialize the PubNub context and set the compulsory parameters
	 * @publish_key and @subscribe_key.
	 *
	 * @cb is a set of callbacks implementing libpubnub's needs for
	 * socket and timer handling, plus default completion callbacks
	 * for the API requests.  Typically, @cb value will be a structure
	 * exported by one of the frontends and @cb_data will be the
	 * appropriate frontend context.  However, you can also customize
	 * these or pass your own structure.
	 *
	 * This function will also initialize the libcurl library. This has
	 * an important connotation for multi-threaded programs, as the first
	 * call to this function will imply curl_global_init() call and that
	 * function is completely thread unsafe. If you need to call
	 * pubnub_init() with other threads already running (and not even
	 * necessarily doing anything PubNub-related), call curl_global_init()
	 * early before spawning other threads.
	 *
	 * One PubNub context can service only a single request at a time
	 * (and you must not access it from multiple threads at once), however
	 * you can maintain as many contexts as you wish. */
	PubNub(const std::string &publish_key, const std::string &subscribe_key,
		const struct pubnub_callbacks *cb, void *cb_data);

	/* You can also create a PubNub wrapper class from an existing pubnub
	 * context. @p_autodestroy determines whether PubNub destructor will
	 * call pubnub_done(p). */
	PubNub(struct pubnub *p, bool p_autodestroy = false);

	/* Deinitialize our PubNub context, freeing all memory that is
	 * associated with it. */
	~PubNub();


	/* Set the secret key that is used for signing published messages
	 * to confirm they are genuine. Using the secret key is optional. */
	void set_secret_key(const std::string &secret_key);

	/* Set the cipher key that is used for symmetric encryption of messages
	 * passed over the network (publish, subscribe, history). Using the
	 * cipher key is optional. */
	void set_cipher_key(const std::string &cipher_key);

	/* Set the origin server name. By default, http://pubsub.pubnub.com/
	 * is used. */
	void set_origin(const std::string &origin);

	/* Retrieve the currently used UUID of this PubNub context. This UUID
	 * is visible to other clients via the here_now call and is normally
	 * autogenerated randomly during pubnub_init(). */
	std::string current_uuid();

	/* Set the UUID of this PubNub context that is asserted during the
	 * subscribe call to identify us. This replaces the autogenerated
	 * UUID. */
	void set_uuid(const std::string &uuid);

	/* This function selects the value of CURLOPT_NOSIGNAL which involves
	 * a tradeoff:
	 *
	 * (i) nosignal is true (DEFAULT) - the library is thread safe and does
	 * not modify signal handlers, however timeout handling will be broken
	 * with regards to DNS requests
	 *
	 * (ii) nosignal is false - DNS requests will be timing out properly,
	 * but the library will install custom SIGPIPE (and possibly SIGCHLD)
	 * handlers and won't be thread safe */
	void set_nosignal(bool nosignal);

	/* Set PubNub error retry policy regarding error handling.
	 *
	 * The call may be retried if the error is possibly recoverable
	 * and retry is enabled for that error. This is controlled by
	 * @retry_mask; if PNR_xxx-th bit is set, the call is retried in case
	 * of the PNR_xxx result; this is the case for recoverable errors,
	 * specifically PNR_OK and PNR_OCCUPIED bits are always ignored (this
	 * may be further extended in the future). For example,
	 *
	 * 	pubnub_error_policy(p, 0, ...);
	 * will turn off automatic error retry for all errors,
	 *
	 * 	pubnub_error_policy(p, ~0, ...);
	 * will turn it on for all recoverable errors (this is the DEFAULT),
	 *
	 * 	pubnub_error_policy(p, ~(1<<PNR_TIMEOUT), ...);
	 * will turn it on for all errors *except* PNR_TIMEOUT, and so on.
	 *
	 * If the call is not automatically retried after an error, the error
	 * is reported to the application via its specified callback instead
	 * (if you use the pubnub_sync frontend, it can be obtained from
	 * pubnub_last_result(); for future compatibility, you should ideally
	 * check it even when relying on automatic error retry).
	 *
	 * A message about the error is printed on stderr if @print is true
	 * (the DEFAULT); this applies even to errors after which we do not
	 * retry for whatever reason. */
	void error_policy(unsigned int retry_mask, bool print);


	/** PubNub API requests */

	/* All the API request functions accept the @timeout [s] parameter
	 * and callback parameters @cb and @cb_data.
	 *
	 * The @timeout [s] parameter describes how long to wait for request
	 * fulfillment before the PNR_TIMEOUT error is generated. Supply -1
	 * if in doubt to obtain the optimal default value. (Note that normally,
	 * PNR_TIMEOUT will just print a message and retry the request; see
	 * pubnub_error_policy() above.)
	 *
	 * If you are using the pubnub_sync frontend, the function calls
	 * will block until the request is fulfilled and you should pass
	 * NULL for @cb and @cb_data. If you are using the pubnub_libevent
	 * or a custom frontend, the function calls will return immediately
	 * and @cb shall point to a function that will be called upon
	 * completion (with @cb_data as its last parameter). */

	/* Publish the @message JSON object on @channel. The response
	 * will usually be just a success confirmation. */
	void publish(const std::string &channel, json_object &message,
			long timeout = -1, PubNub_publish_cb cb = NULL, void *cb_data = NULL);

	/* Subscribe to @channel. The response will be a JSON array with
	 * one received message per item.
	 *
	 * Messages published on @channel since the last subscribe call
	 * are returned.  The first call will typically just establish
	 * the context and return immediately with an empty response array.
	 * Usually, you would issue the subscribe request in a loop. */
	void subscribe(const std::string &channel,
			long timeout = -1, PubNub_subscribe_cb cb = NULL, void *cb_data = NULL);

	/* Subscribe to a set of @channels. The response will contain
	 * received messages from any of these channels; use the channels
	 * callback parameter (or pubnub_sync_last_channels()) to determine
	 * the originating channel of each message. */
	void subscribe_multi(const std::vector<std::string> &channels,
			long timeout = -1, PubNub_subscribe_cb cb = NULL, void *cb_data = NULL);

	/* List the last @limit messages that appeared on a @channel.
	 * You do not need to be subscribed to the channel. The response
	 * will be a JSON array with one message per item. */
	void history(const std::string &channel, int limit,
			long timeout = -1, PubNub_history_cb cb = NULL, void *cb_data = NULL);

	/* Like history(), but with the added options.
	 * @include_token the value of the `include_token` URL parameter to send */
	void history_ex(const std::string &channel, int limit, bool include_token,
			long timeout = -1, PubNub_history_cb cb = NULL, void *cb_data = NULL);

	/* List the clients subscribed to @channel. The response will be
	 * a JSON object with attributes "occupancy" (number of clients)
	 * and "uuids" (array of client UUIDs). */
	void here_now(const std::string &channel,
			long timeout = -1, PubNub_here_now_cb cb = NULL, void *cb_data = NULL);

	/* Retrieve the server timestamp (number of microseconds since
	 * 1970-01-01), stored as JSON value in the response. You can use
	 * this as a sort of "ping" message e.g. to estimate network lag,
	 * or if you do not trust the system time. */
	void time(long timeout = -1, PubNub_time_cb cb = NULL, void *cb_data = NULL);

protected:
	struct pubnub *p;
	bool p_autodestroy;
};

#endif
