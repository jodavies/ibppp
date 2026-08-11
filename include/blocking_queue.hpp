#ifndef BLOCKING_QUEUE_H
#define BLOCKING_QUEUE_H

#include <format>
#include <condition_variable>
#include <queue>
#include <mutex>

// #[ class blocking_queue
//
// A simple blocking queue with a maximum capacity, for parallel reading and
// processing/writing in streaming functions.

template<typename T>
class blocking_queue {

	private:

		std::mutex prot;
		std::condition_variable has_items_flag;
		std::condition_variable free_space_flag;

		std::queue<T> queue;
		std::size_t max_queue_size;
		bool done = false;

	public:

		blocking_queue(std::size_t max_size)
			: max_queue_size(max_size) {
			if ( max_queue_size == 0 ) {
				throw std::invalid_argument("blocking_queue requires max size > 0");
			}
		}

		void push(T item) {

			std::unique_lock lock(prot);
			free_space_flag.wait(lock, [&] {
				return done || queue.size() < max_queue_size;
			});

			if ( done ) {
				throw std::runtime_error(
					std::format("{}: error: push on closed queue", __func__)
				);
			}

			queue.push(std::move(item));

			has_items_flag.notify_one();
		}

		bool pop(T& item) {

			std::unique_lock lock(prot);
			has_items_flag.wait(lock, [&] {
				return done || ! queue.empty();
			});

			if ( queue.empty() ) {
				return false;
			}

			item = std::move(queue.front());
			queue.pop();

			free_space_flag.notify_one();

			return true;
		}

		// No further items can be pushed to the queue.
		// Existing items remain and can be popped until the queue is empty.
		void close() {
			std::lock_guard lock(prot);
			done = true;
			has_items_flag.notify_all();
		}
};
//#]

#endif
