#ifndef VN_SCALED_INT_H
#define VN_SCALED_INT_H

// Scaled integer operations for Van Nueman
// Used by vncc compiler

template<typename T, int SCALE>
class ScaledInt {
public:
    T raw_value;
    
    ScaledInt() : raw_value(0) {}
    ScaledInt(float f) : raw_value(static_cast<T>(f * SCALE)) {}
    
    float to_float() const { return static_cast<float>(raw_value) / static_cast<float>(SCALE); }
    void set_float(float f) { raw_value = static_cast<T>(f * SCALE); }
    
    ScaledInt operator+(const ScaledInt& other) const {
        ScaledInt result;
        result.raw_value = raw_value + other.raw_value;
        return result;
    }
};

#endif // VN_SCALED_INT_H
