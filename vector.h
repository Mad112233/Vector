#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <algorithm>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    explicit RawMemory(size_t capacity) : buffer_(Allocate(capacity)), capacity_(capacity) {}
    RawMemory(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::move(other.buffer_);
        capacity_ = std::move(other.capacity_);
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        buffer_ = std::move(rhs.buffer_);
        capacity_ = std::move(rhs.capacity_);

        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size) : data_(size), size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other) : data_(other.size_), size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    void Resize(size_t new_size) {
        if (new_size <= size_) {
            std::destroy_n(&data_[new_size], size_ - new_size);
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(&data_[size_], new_size - size_);
        }
        size_ = new_size;
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        const size_t idx = pos - begin();

        if (size_ == Capacity()) {
            RawMemory<T>new_data(size_ == 0 ? 1 : size_ * 2);            

            new (&new_data[idx])T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                try {
                    std::uninitialized_move_n(data_.GetAddress(), idx, new_data.GetAddress());
                }
                catch (...) {
                    std::destroy_n(&new_data[idx], 1);
                    throw;
                }

                try {
                    std::uninitialized_move_n(data_.GetAddress() + idx, size_ - idx, new_data.GetAddress() + idx + 1);
                }
                catch (...) {
                    std::destroy_n(new_data.GetAddress(), idx);
                    throw;
                }
            }
            else {
                try {
                    std::uninitialized_copy_n(data_.GetAddress(), idx, new_data.GetAddress());
                }
                catch (...) {
                    std::destroy_n(&new_data[idx], 1);
                    throw;
                }

                try {
                    std::uninitialized_copy_n(data_.GetAddress() + idx, size_ - idx, new_data.GetAddress() + idx + 1);
                }
                catch (...) {
                    std::destroy_n(new_data.GetAddress(), idx);
                    throw;
                }
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            if (size_) {
                new (end())T(std::move(data_[size_ - 1]));
                std::move_backward(begin() + idx, end() - 1, end());
            }

            if (idx < size_)
                data_[idx] = std::move(T(std::forward<Args>(args)...));
            else
                new(&data_[idx])T(std::forward<Args>(args)...);
        }

        ++size_;
        return &data_[idx];
    }
    
    iterator Erase(const_iterator pos) {
        const size_t idx = pos - begin();
        std::move(begin() + idx + 1, end(), begin() + idx);
        std::destroy_n(begin() + size_ - 1, 1);
        --size_;
        return begin() + idx;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T>new_data(size_ == 0 ? 1 : size_ * 2);

            new (&new_data[size_])T(std::forward<Args>(args)...);

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            else
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            new (&data_[size_])T(std::forward<Args>(args)...);
        }

        ++size_;
        return data_[size_ - 1];
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() {
        std::destroy_n(&data_[size_ - 1], 1);
        --size_;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (size_ > rhs.size_) {
                    size_t i = 0;
                    for (; i < rhs.size_; ++i) {
                        data_[i] = rhs[i];
                    }
                    std::destroy_n(&data_[i], size_ - rhs.size_);
                }
                else {
                    size_t i = 0;
                    for (; i < size_; ++i) {
                        data_[i] = rhs[i];
                    }
                    for (; i < rhs.size_; ++i) {
                        new(&data_[i])T(rhs[i]);
                    }
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity())
            return;

        RawMemory<T>new_data(new_capacity);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>)
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        else
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};