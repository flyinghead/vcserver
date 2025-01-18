/*
	Copyright 2024 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/
#pragma once
#include <memory>

template<typename T>
class SharedThis : public std::enable_shared_from_this<T>
{
public:
	using Ptr = std::shared_ptr<T>;

	template<typename... Args>
	static Ptr create(Args&&... args) {
		return Ptr(new T(std::forward<Args>(args)...));
	}

protected:
	using super = SharedThis<T>;

	SharedThis() {
	}
};
