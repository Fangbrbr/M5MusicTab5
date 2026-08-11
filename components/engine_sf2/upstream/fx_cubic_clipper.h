#include <cmath>
#include <algorithm>

class CubicSoftClipper {
private:
    // Parameters
    float pre_gain_ = 4.0f;       // Input boost (1.0-20.0)
    float post_gain_ = 1.0f;      // Output compensation (auto-calculated)
    float threshold_ = 0.8f;      // Soft clipping threshold (0.5-1.0)
    
    // State
    float auto_comp_ = 1.0f;      // Auto compensation factor
    bool auto_gain_ = true;       // Auto gain compensation flag

public:
    void init(float sample_rate) {
        // Initialize any needed state
        update_compensation();
    }

    // Main processing (branchless)
    float process_sample(float x) {
        const float dry = x;
        
        // Apply pre-gain and soft clipping
        x *= pre_gain_;
        
        // Cubic soft clipping (branchless)
        float abs_x = std::fabs(x);
        float x2 = abs_x * abs_x;
        float x3 = x2 * abs_x;
        
        // Normalized cubic curve (range -1.33 to +1.33)
        float y = (1.5f * abs_x) - (0.5f * x3 / threshold_);
        y = std::min(y, threshold_);  // Clamp to threshold
        
        // Restore polarity and apply compensation
        y = std::copysign(y, x) * post_gain_;
         
        return y;
    }

    void process(float* left, float* right) {
        *left = process_sample(*left);
        *right = process_sample(*right);
    }

    // Parameter setters with validation
    void set_pre_gain(float gain_db) {
        // Convert dB to linear (6dB = 2x)
        pre_gain_ = std::pow(10.0f, gain_db / 20.0f);
        update_compensation();
    }

    void set_threshold(float threshold) {
        threshold_ = std::clamp(threshold, 0.5f, 1.0f);
        update_compensation();
    }
 
    void set_auto_gain(bool enabled) {
        auto_gain_ = enabled;
        update_compensation();
    }

    void set_post_gain(float gain_db) {
        auto_gain_ = false;
        post_gain_ = std::pow(10.0f, gain_db / 20.0f);
    }

private:
    void update_compensation() {
        if (!auto_gain_) return;
        
        // Calculate expected peak reduction
        const float test_input = 1.0f;  // Unity input
        const float clipped = process_sample(test_input / pre_gain_) * pre_gain_;
        
        // Compensation factor to maintain similar peak levels
        auto_comp_ = (clipped != 0.0f) ? (test_input / clipped) : 1.0f;
        post_gain_ = auto_comp_;
    }
};