#pragma once

/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#include <imagine/config/defs.hh>
#include <imagine/base/Pipe.hh>
#include <imagine/thread/Semaphore.hh>
#include <imagine/util/concepts.hh>
#include <imagine/util/utility.h>
#include <utility>
#include <cstring>

namespace Base
{

template <class MsgType>
concept ReplySemaphoreSettableMessage =
	requires (MsgType msg, IG::Semaphore *sem){ msg.setReplySemaphore(sem); };

template<class MsgType>
class PipeMessagePort
{
public:
	class Messages
	{
	public:
		class Iterator
		{
		public:
			constexpr Iterator(IO *io): io{io}
			{
				if(!io)
					return;
				this->operator++();
			}

			Iterator operator++()
			{
				msg = io->get<MsgType>();
				if(!msg)
				{
					// end of messages
					io = nullptr;
				}
				return *this;
			}

			bool operator!=(const Iterator &rhs) const
			{
				return io != rhs.io;
			}

			const MsgType &operator*() const
			{
				return msg;
			}

		private:
			IO *io;
			MsgType msg;
		};

		constexpr Messages(IO &io): io{io} {}

		Iterator begin() { return Iterator{&io}; }
		Iterator end() { return Iterator{nullptr}; }

		template <class T>
		T getExtraData()
		{
			return io.get<T>();
		}

		bool getExtraData(auto *obj, size_t size)
		{
			return io.read(obj, size) != -1;
		}

	protected:
		IO &io;
	};

	static constexpr uint32_t MSG_SIZE = sizeof(MsgType);
	static_assert(MSG_SIZE < PIPE_BUF, "size of message too big for atomic writes");

	struct NullInit{};

	PipeMessagePort(const char *debugLabel = nullptr, uint32_t capacity = 8):
		pipe{debugLabel, MSG_SIZE * capacity}
	{
		pipe.setReadNonBlocking(true);
	}

	explicit constexpr PipeMessagePort(NullInit) {}

	void attach(auto &&f)
	{
		attach(EventLoop::forThread(), std::forward<decltype(f)>(f));
	}

	void attach(EventLoop loop, IG::Callable<void, Messages> auto &&f)
	{
		pipe.attach(loop,
			[=](auto &io) -> bool
			{
				Messages msg{io};
				f(msg);
				return true;
			});
	}

	void attach(EventLoop loop, IG::Callable<bool, Messages> auto &&f)
	{
		pipe.attach(loop,
			[=](auto &io) -> bool
			{
				Messages msg{io};
				return f(msg);
			});
	}

	void detach()
	{
		pipe.detach();
	}

	bool send(MsgType msg)
	{
		return pipe.sink().write(msg) != -1;
	}

	bool send(MsgType msg, bool awaitReply)
	{
		if(awaitReply)
		{
			IG::Semaphore sem{0};
			return send(msg, &sem);
		}
		else
		{
			return send(msg);
		}
	}

	bool send(MsgType msg, IG::Semaphore *semPtr) requires ReplySemaphoreSettableMessage<MsgType>
	{
		if(semPtr)
		{
			msg.setReplySemaphore(semPtr);
			if(pipe.sink().write(msg) == -1) [[unlikely]]
			{
				return false;
			}
			semPtr->wait();
			return true;
		}
		else
		{
			return send(msg);
		}
	}

	bool sendWithExtraData(MsgType msg, auto &&obj)
	{
		static_assert(MSG_SIZE + sizeof(obj) < PIPE_BUF, "size of data too big for atomic writes");
		return sendWithExtraData(msg, &obj, sizeof(obj));
	}

	bool sendWithExtraData(MsgType msg, auto *obj, uint32_t size)
	{
		const auto bufferSize = MSG_SIZE + size;
		assumeExpr(bufferSize < PIPE_BUF);
		char buffer[bufferSize];
		memcpy(buffer, &msg, MSG_SIZE);
		memcpy(buffer + MSG_SIZE, obj, size);
		return pipe.sink().write(buffer, bufferSize) != -1;
	}

	void clear()
	{
		auto &io = pipe.source();
		while(io.template get<MsgType>()) {}
	}

	void dispatchMessages()
	{
		pipe.dispatchSourceEvents();
	}

	explicit operator bool() const { return (bool)pipe; }

protected:
	Pipe pipe{Pipe::NullInit{}};
};

template<class MsgType>
using MessagePort = PipeMessagePort<MsgType>;

}
