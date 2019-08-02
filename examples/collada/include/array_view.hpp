#ifndef __ARRAY_VIEW_HPP_INCLUDED__
#define __ARRAY_VIEW_HPP_INCLUDED__

namespace util {

/// A non-owning reference to an raw array
template<class T>
class array_view
{
	public:
		constexpr array_view(const T* array, const std::size_t size) noexcept:
			px_(array),
			size_(size)
		{}
		const T* get() const noexcept {
			return px_;
		}
		const std::size_t size() noexcept {
			return size_;
		}
		const std::size_t bytes() noexcept {
			return sizeof(T) * size_;
		}
	protected:
		const T* px_;
		const std::size_t size_;
};

} // namespace util

#endif // __ARRAY_VIEW_HPP_INCLUDED__
