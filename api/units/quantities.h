/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_UNITS_QUANTITIES_H_
#define API_UNITS_QUANTITIES_H_

#include <type_traits>

#define ONLY_ALLOW_DEFINED_QUANTITIES 1
#define DIMENSIONLESS_QUANTITY_IS_DOUBLE 1

namespace webrtc {

template <int T, int S>
class Quantity;

template <int T, int S>
struct DimsToQuantity {
#if !ONLY_ALLOW_DEFINED_QUANTITIES
  using type = Quantity<T, S>;
#endif
};

template <>
struct DimsToQuantity<0, 0> {
#if DIMENSIONLESS_QUANTITY_IS_DOUBLE
  using type = double;
#else
  using type = Quantity<0, 0>;
#endif
};

template <int T, int S>
using DimsToQuantityType = typename DimsToQuantity<T, S>::type;

template <int T, int S>
class Quantity {
 public:
#if !DIMENSIONLESS_QUANTITY_IS_DOUBLE
  // Need some way to access the value when the quantity is dimensionless.
  // Maybe a Value() function instead?
  template <bool IsDimensionless = T == 0 && S == 0,
            std::enable_if_t<IsDimensionless, bool> = true>
  constexpr operator double() const {
    return val_;
  }
#endif

  constexpr bool operator==(Quantity<T, S> u) const { return val_ == u.val_; }
  constexpr bool operator!=(Quantity<T, S> u) const { return val_ != u.val_; }
  constexpr bool operator<(Quantity<T, S> u) const { return val_ < u.val_; }
  constexpr bool operator<=(Quantity<T, S> u) const { return val_ <= u.val_; }
  constexpr bool operator>(Quantity<T, S> u) const { return val_ > u.val_; }
  constexpr bool operator>=(Quantity<T, S> u) const { return val_ >= u.val_; }

  template <int Tu, int Su>
  friend constexpr DimsToQuantityType<Tu, Su> operator+(Quantity<Tu, Su> lhs,
                                                        Quantity<Tu, Su> rhs);

  template <int Tu, int Su>
  friend constexpr DimsToQuantityType<Tu, Su> operator-(Quantity<Tu, Su> lhs,
                                                        Quantity<Tu, Su> rhs);

  template <int Tu, int Su>
  friend constexpr DimsToQuantityType<Tu, Su> operator*(double scalar,
                                                        Quantity<Tu, Su> rhs);

  template <int Tu, int Su>
  friend constexpr DimsToQuantityType<Tu, Su> operator*(Quantity<Tu, Su> lhs,
                                                        double scalar);

  template <int Tl, int Sl, int Tr, int Sr>
  friend constexpr DimsToQuantityType<Tl + Tr, Sl + Sr> operator*(
      Quantity<Tl, Sl> lhs,
      Quantity<Tr, Sr> rhs);

  template <int Tl, int Sl, int Tr, int Sr>
  friend constexpr DimsToQuantityType<Tl - Tr, Sl - Sr> operator/(
      Quantity<Tl, Sl> lhs,
      Quantity<Tr, Sr> rhs);

 protected:
  constexpr explicit Quantity(double val) : val_(val) {}
  double val_;
};

template <int Tu, int Su>
constexpr DimsToQuantityType<Tu, Su> operator+(Quantity<Tu, Su> lhs,
                                               Quantity<Tu, Su> rhs) {
  return Quantity<Tu, Su>(lhs.val_ + rhs.val_);
}

template <int Tu, int Su>
constexpr DimsToQuantityType<Tu, Su> operator-(Quantity<Tu, Su> lhs,
                                               Quantity<Tu, Su> rhs) {
  return Quantity<Tu, Su>(lhs.val_ - rhs.val_);
}

template <int Tu, int Su>
constexpr DimsToQuantityType<Tu, Su> operator*(double scalar,
                                               Quantity<Tu, Su> rhs) {
  return Quantity<Tu, Su>(rhs.val_ * scalar);
}

template <int Tu, int Su>
constexpr DimsToQuantityType<Tu, Su> operator*(Quantity<Tu, Su> lhs,
                                               double scalar) {
  return Quantity<Tu, Su>(lhs.val_ * scalar);
}

template <int Tl, int Sl, int Tr, int Sr>
constexpr DimsToQuantityType<Tl + Tr, Sl + Sr> operator*(Quantity<Tl, Sl> lhs,
                                                         Quantity<Tr, Sr> rhs) {
  if constexpr (std::is_same_v<DimsToQuantityType<Tl + Tr, Sl + Sr>, double>) {
    return lhs.val_ * rhs.val_;
  } else {
    return Quantity<Tl + Tr, Sl + Sr>(lhs.val_ * rhs.val_);
  }
}

template <int Tl, int Sl, int Tr, int Sr>
constexpr DimsToQuantityType<Tl - Tr, Sl - Sr> operator/(Quantity<Tl, Sl> lhs,
                                                         Quantity<Tr, Sr> rhs) {
  if constexpr (std::is_same_v<DimsToQuantityType<Tl - Tr, Sl - Sr>, double>) {
    return lhs.val_ / rhs.val_;
  } else {
    return Quantity<Tl - Tr, Sl - Sr>(lhs.val_ / rhs.val_);
  }
}

class DataSize : public Quantity<0, 1> {
 public:
  constexpr DataSize(const Quantity<0, 1>& u) : Quantity<0, 1>(u) {}

  static constexpr DataSize Bits(double val) { return DataSize(val); }
  static constexpr DataSize Bytes(double val) { return DataSize(val * 8); }
  constexpr double Bits() const { return val_; }
  constexpr double Bytes() const { return val_ / 8.0; }

 private:
  constexpr explicit DataSize(double val) : Quantity<0, 1>(val) {}
};

template <>
struct DimsToQuantity<0, 1> {
  using type = DataSize;
};

class DataRate : public Quantity<-1, 1> {
 public:
  constexpr DataRate(Quantity<-1, 1> u) : Quantity<-1, 1>(u) {}
  static constexpr DataRate BitsPerSec(double val) { return DataRate(val); }
  static constexpr DataRate BytesPerSec(double val) {
    return DataRate(val * 8);
  }

  constexpr double BitsPerSec() const { return val_; }
  constexpr double BytesPerSec() const { return val_ / 8; }

 private:
  constexpr explicit DataRate(double val) : Quantity<-1, 1>(val) {}
};

template <>
struct DimsToQuantity<-1, 1> {
  using type = DataRate;
};

class TimeDelta : public Quantity<1, 0> {
 public:
  constexpr TimeDelta(Quantity<1, 0> u) : Quantity<1, 0>(u) {}
  static constexpr TimeDelta Seconds(double val) { return TimeDelta(val); }
  static constexpr TimeDelta Millis(double val) {
    return TimeDelta(val / 1'000);
  }
  static constexpr TimeDelta Micros(double val) {
    return TimeDelta(val / 1'000'000);
  }

  constexpr double seconds() const { return val_; }
  constexpr double ms() const { return val_ * 1'000; }
  constexpr double us() const { return val_ * 1'000'000; }

 private:
  constexpr explicit TimeDelta(double val) : Quantity<1, 0>(val) {}
};

template <>
struct DimsToQuantity<1, 0> {
  using type = TimeDelta;
};

class Frequency : public Quantity<-1, 0> {
 public:
  constexpr Frequency(Quantity<-1, 0> u) : Quantity<-1, 0>(u) {}
  static constexpr Frequency Hz(double val) { return Frequency(val); }
  static constexpr Frequency KiloHz(double val) {
    return Frequency(val * 1'000);
  }

  constexpr double hz() const { return val_; }
  constexpr double khz() const { return val_ / 1'000; }

 private:
  constexpr explicit Frequency(double val) : Quantity<-1, 0>(val) {}
};

template <>
struct DimsToQuantity<-1, 0> {
  using type = Frequency;
};

}  // namespace webrtc

#endif  // API_UNITS_QUANTITIES_H_
