#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <type_traits>
#include <vector>


// === Animal Hierarchy ===

class Animal
{
public:
    explicit Animal(std::string name) : name_{std::move(name)} {}

    virtual ~Animal() = default;

    virtual void print_food_requirements() const
    {
        std::cout << name_ << " needs: ";
    }

    [[nodiscard]] const std::string& get_name() const
    {
        return name_;
    }

protected:
    std::string name_;
};

class Elephant : public Animal
{
public:
    Elephant() : Animal{"Elephant"} {}

    void print_food_requirements() const override
    {
        Animal::print_food_requirements();
        std::cout << "hay, fruit, vegetables (300kg/day)";
    }
};

class Zebra : public Animal
{
public:
    Zebra() : Animal{"Zebra"} {}

    void print_food_requirements() const override
    {
        Animal::print_food_requirements();
        std::cout << "hay, grass (15kg/day)";
    }
};

class Lion : public Animal
{
public:
    Lion() : Animal{"Lion"} {}

    void print_food_requirements() const override
    {
        Animal::print_food_requirements();
        std::cout << "meat (11kg/day)";
    }
};

class Penguin : public Animal
{
public:
    Penguin() : Animal{"Penguin"} {}

    void print_food_requirements() const override
    {
        Animal::print_food_requirements();
        std::cout << "fish (2kg/day)";
    }
};

// === The Fixed-Size Wrapper using Type Erasure

class AnyAnimal
{
    static constexpr size_t BUFFER_SIZE = 128;
    static constexpr size_t ALIGNMENT   = alignof(std::max_align_t);

    struct VTable
    {
        void (*print_food_requirements)(const void* data);
        void (*destroy)(void* data); // Removed 'const'
        void (*move)(void* src, void* dst);
    };

    alignas(ALIGNMENT) std::byte storage_[BUFFER_SIZE]{};
    const VTable* vptr{};

    template <typename T>
    static constexpr VTable vtable_for{
        .print_food_requirements =
            [](const void* data)
        {
            const T* ptr = static_cast<const T*>(data);
            ptr->print_food_requirements();
        },

        .destroy = [](void* ptr) { std::destroy_at(static_cast<T*>(ptr)); },

        .move =
            [](void* src, void* dst)
        {
            T* src_ptr = static_cast<T*>(src);
            new (dst) T(std::move(*src_ptr));
            std::destroy_at(src_ptr);
        }};

public:
    template <
        typename T,
        // Prevent this constructor from shadowing copy/move constructors
        typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, AnyAnimal>>>
    AnyAnimal(T obj) : vptr{&vtable_for<T>}
    {
        static_assert(sizeof(T) <= BUFFER_SIZE, "Object too big");
        static_assert(alignof(T) <= ALIGNMENT, "Alignment error");
        new (storage_) T(std::move(obj));
    }

    ~AnyAnimal()
    {
        if (vptr)
        {
            // Pass non-const storage_
            vptr->destroy(storage_);
        }
    }

    AnyAnimal(AnyAnimal&& other) noexcept
    {
        if (other.vptr)
        {
            other.vptr->move(other.storage_, storage_);
            this->vptr = other.vptr;
            other.vptr = nullptr;
        }
        else
        {
            vptr = nullptr;
        }
    }

    AnyAnimal& operator=(AnyAnimal&& other) noexcept
    {
        if (this == &other) return *this;

        // Destroy current
        if (vptr) vptr->destroy(storage_);

        // Move new
        if (other.vptr)
        {
            other.vptr->move(other.storage_, storage_);
            this->vptr = other.vptr;
            other.vptr = nullptr;
        }
        else
        {
            vptr = nullptr;
        }
        return *this;
    }

    AnyAnimal(const AnyAnimal&)            = delete;
    AnyAnimal& operator=(const AnyAnimal&) = delete;

    void print_food_requirements() const
    {
        assert(vptr && "Empty AnyAnimal");
        vptr->print_food_requirements(storage_);
    }
};

int main()
{
    // Use a static vector here to avoid runtime allocations.
    std::vector<AnyAnimal> animal_roster;

    animal_roster.emplace_back(Lion{});
    animal_roster.emplace_back(Zebra{});
    animal_roster.emplace_back(Elephant{});

    for (const auto& animal : animal_roster)
    {
        animal.print_food_requirements();
        std::cout << std::endl;
    }

    return 0;
}
