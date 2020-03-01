#pragma once

#include <cassert>

#include <algorithm>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <list>
#include <memory>
#include <numeric>
#include <string>
#include <thread>

#include "Names.hh"

#include "ge/Vector.hh"


template <typename T>
using Constraint = std::function<T(T const &_value)>;

template <typename T>
struct ValueProperty {
  std::list<Constraint<T>> constraints;
  using ConstraintID = typename decltype(constraints)::const_iterator;

  void applyConstraints() noexcept {
    this->value = std::accumulate(constraints.begin(), constraints.end(), this->value,
                      [](T const &_acc, Constraint<T> &foo) {
                        return foo(_acc);
                      });
  }

  T get() {
    this->applyConstraints();
    return this->value;
  }

  explicit operator T const &() const noexcept {
    return this->value;
  }

  T const &set(T const &_value) {
    throw std::runtime_error("invalid operation");
    return this->value;
  }

  ConstraintID addConstraint(Constraint<T> const &_constraint) {
    this->constraints.push_back(_constraint);
    return std::prev(this->constraints.cend(), 1);
  }

  void removeConstraint(ConstraintID _cid) {
    this->constraints.erase(_cid);
  }

  protected: T value {};
};

template <typename T>
struct Property: public ValueProperty<T> {
  T const &set(T const &_value) {
    return this->value = _value;
  }

  // T const &operator = (ValueProperty<T> &_other) {
  //   return this->set(_other.get());
  // }

  T const &operator = (T const &_value) {
    return this->set(_value);
  }

  T const &operator += (T const &_value) {
    return this->set(this->value + _value);
  }

  T const &operator -= (T const &_value) {
    return this->set(this->value - _value);
  }

  Property() = default;
  Property(T const &_value) {
    this->set(_value);
  }
};

template <typename T>
struct DependentProperty: public ValueProperty<T> {
  // DependentProperty(Constraint<T> _primary_constraint) {
  //   this->constraints.push_back(_primary_constraint);
  // }
};

class Rect {
  public: Property<float> left;
  public: Property<float> top;
  public: Property<float> width;
  public: Property<float> height;
  public: DependentProperty<float> right;
  public: DependentProperty<float> bottom;

  public: Rect() {
    this->setConstraints();
  }

  public: std::tuple<float, float, float, float> asTuple() {
    return std::tuple(
        this->left.get(),
        this->top.get(),
        this->right.get(),
        this->bottom.get());
  }

  private: void setConstraints() {
    this->right.constraints.push_back(
        [&left = this->left, &width = this->width](float const &_val) {
          return left.get()+width.get();});
    this->bottom.constraints.push_back(
        [&top = this->top, &height = this->height](float const &_val) {
          return top.get()+height.get();});
  }
};

struct Buffer {
  void *data;
  size_t size;
};

class Surface
{
  public: std::string id;
  public: Rect rect;
  public: Buffer data;
};

class Compositor {
  public: std::list<std::weak_ptr<Surface>> surfaces;
  public: void compose() {
    for (auto &&wptr : this->surfaces) {
      assert(!wptr.expired());
      auto surface = wptr.lock();
      assert(surface);

      auto [x1, y1, x2, y2] = surface->rect.asTuple();

      std::cout << "drawRect '" << surface->id << "' {" << x1 << ":" << x2 << "|" << y1 << ":" << y2 << "};" << std::endl;
    }
  }

  public: void present() {
    std::cout << "Present()" << std::endl;
  }

  static std::unique_ptr<Surface> makeSurface() {
    static size_t counter = 0;
    if (counter >= names::strs_num)
      throw "Out of names";

    auto surface = std::make_unique<Surface>();
    surface->id = names::strs[counter++];

    return surface;
  }
};

class Grab {
  public: std::shared_ptr<Surface> surface;
  public: Vec2f origin {};
  public: Vec2f offset {};

  public: Grab(std::shared_ptr<Surface> _surface, Vec2f const _origin)
  : surface {_surface}
  , origin {_origin} {
  }

  public: ~Grab() {
    surface->rect.left += offset.x;
    surface->rect.top += offset.y;
  }

  // private: std::list<Property<float>::ConstraintID> created_constraints;
};

template <typename T>
class Transition {
  public: Transition(Property<T> &_property, T _target_value)
  : property {_property} {
    this->transition_constraint_id =
        this->property.addConstraint(
            [&phase = this->phase,
             target_value = _target_value](T const &_val) {
              auto inter_val = _val * (1.f - phase) + target_value * phase;
              /*std::cout << "Apply constraint: phase " << phase
                        << " | " << _val << " -> " << inter_val
                        << "(" << target_value << ") |;" << std::endl;*/
              return inter_val;
            });
  }

  public: ~Transition() {
    this->property.removeConstraint(this->transition_constraint_id);
  }

  public: void setPhase(float const _phase) noexcept {
    this->phase = _phase;
  }

  private: Property<T> &property;
  protected: float phase {0.f};

  private: typename Property<T>::ConstraintID transition_constraint_id;
};

template <typename Rep, typename Period>
std::tuple<std::future<void>, std::promise<void>>
startTimer(
    std::chrono::duration<Rep, Period> const &_interval,
    std::function<void(int const _iter)> _handler) {
  std::promise<void> brake;
  auto timer_handle = std::async(std::launch::async,
      [interval = _interval,
       done = brake.get_future(),
       handler = _handler]() {
        int i {0};
        while (done.wait_for(interval) != std::future_status::ready) {
          handler(i++);
        };
      });

  return std::tuple(std::move(timer_handle), std::move(brake));
}

template <typename T>
class TimedTransition : public Transition<T> {
  public: template <typename Rep, typename Period>
  TimedTransition(
      Property<T> &_property, T _target_value,
      std::chrono::duration<Rep, Period> const &_duration,
      int const _steps)
  : Transition<T> {_property, _target_value} {
    std::tie(timer_handle, brake) =
        startTimer(
            _duration / _steps,
            [this, _steps](int const _iter) {
              if (_iter == _steps)
                this->cancel();

              this->setPhase((_iter + 1.f) / (_steps + 1));
              std::cout << "transition_timer: " << _iter << ":" << this->phase << std::endl;
            });
  }

  public: void cancel() {
    try {
      this->brake.set_value();
    } catch (std::future_error const &_err) {
      // swallow
    }
  }

  public: void wait() {
    timer_handle.wait();
  }

  public: ~TimedTransition() {
    this->wait();
  }

  private: std::future<void> timer_handle;
  private: std::promise<void> brake;
};

namespace constraints {
template <typename T>
struct Follow {
  Follow(ValueProperty<T> &_target)
  : target {_target} {
  }

  T operator ()(T const &_val) {
    return this->target.get();
  }

  private: ValueProperty<T> &target;
};

template <typename T>
struct CenterIn {
  CenterIn(ValueProperty<T> &_a, ValueProperty<T> &_b)
  : a {_a}
  , b {_b} {
  }

  T operator ()(T const &_val) {
    return (this->a.get() + this->b.get()) / 2;
  }

  private: ValueProperty<T> &a;
  private: ValueProperty<T> &b;
};

template <typename T>
struct OffsetFor {
  OffsetFor(ValueProperty<T> &_a, ValueProperty<T> &_b)
  : a {_a}
  , b {_b} {
  }

  T operator ()(T const &_val) {
    return (this->a.get() + this->b.get());
  }

  private: ValueProperty<T> &a;
  private: ValueProperty<T> &b;
};

}

int main() {
  std::shared_ptr<Surface> s_a {Compositor::makeSurface()};

  s_a->rect.left = 10;
  s_a->rect.top = 20;
  s_a->rect.width = 320;
  s_a->rect.height = 240;

  Compositor compositor;
  compositor.surfaces.push_back(s_a);

  compositor.compose();
  compositor.present();

  {
    Grab grab {s_a, {10.f, 10.f}};
    grab.offset = grab.offset + Vec2f{20.f, 15.f};
  }

  compositor.compose();
  compositor.present();

  Property<float> p1 {0.f};
  {
    std::cout << "p1: " << p1.get() << std::endl;
    Transition<float> transition(std::ref(p1), 1.f);
    std::cout << "p1: " << p1.get() << std::endl;
    transition.setPhase(0.5f);
    std::cout << "p1: " << p1.get() << std::endl;
    transition.setPhase(1.f);
    std::cout << "p1: " << p1.get() << std::endl;
  }

  {
    Transition<float> transition(std::ref(p1), 0.f);
    auto [timer, brake] =
        startTimer(
            std::chrono::milliseconds(100),
            [&transition, &p1](int const _iter) {
              transition.setPhase((_iter + 1) / 10.0);
              std::cout << "p1: " << p1.get() << std::endl;
            });

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    brake.set_value();
  }

  {
    using namespace std::literals;
    auto transition = TimedTransition(p1, 10.f, 1000ms, 20);
    std::this_thread::sleep_for(600ms);
    transition.cancel();
  }

  {
    Property<float> p2 {0.f};
    Property<float> p3 {0.f};
    p3.addConstraint(constraints::Follow(p2));
    std::cout << "p2: " << p2.get() << std::endl;
    std::cout << "p3: " << p3.get() << std::endl;

    p2.set(10);

    std::cout << "p2: " << p2.get() << std::endl;
    std::cout << "p3: " << p3.get() << std::endl;
  }

  return 0;
}
