#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>


/**
 * @brief Small constant used to prevent exact zero values in random generation.
 */
const double ZERO = 1e-13;

/**
 * @brief Provides basic file I/O operations specifically for CSV files.
 */
namespace file_manager::csv {

    /**
     * @brief Reads a CSV file into a 2D vector of strings.
     * @param file_path The full path to the file to be read.
     * @param delimiter The character that separates columns, defaults to comma.
     * @return An optional containing a 2D vector of strings upon success, or std::nullopt on failure.
     */
    inline std::optional<std::vector<std::vector<std::string>>> read(const std::string& file_path, char delimiter = ',') {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open file " << file_path << "\n";
            return std::nullopt;
        }

        std::vector<std::vector<std::string>> file_data;
        std::string line;

        while (std::getline(file, line)) {
            std::vector<std::string> row;
            std::stringstream ss(line);
            std::string cell;

            while (std::getline(ss, cell, delimiter)) {
                row.push_back(cell);
            }
            
            if (!row.empty()) {
                file_data.push_back(row);
            }
        }
        return file_data;
    }

    /**
     * @brief Writes a 2D vector of strings to a CSV file.
     * @param file_path The full path to the output file.
     * @param data The 2D vector of strings to write.
     * @param delimiter The character used for separating columns, defaults to comma.
     * @return True if successful, false otherwise.
     */
    inline bool write(const std::string& file_path, const std::vector<std::vector<std::string>>& data, char delimiter = ',') {
        std::filesystem::path path(file_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Error: Could not create or open file " << file_path << "\n";
            return false;
        }

        for (size_t i = 0; i < data.size(); ++i) {
            for (size_t j = 0; j < data[i].size(); ++j) {
                file << data[i][j];
                if (j < data[i].size() - 1) {
                    file << delimiter;
                }
            }
            if (i < data.size() - 1) {
                file << "\n";
            }
        }
        return true;
    }

    /**
     * @brief Appends a 2D vector of strings to a CSV file.
     * @param file_path The full path to the output file.
     * @param data The 2D vector of strings to append.
     * @param delimiter The character used for separating columns, defaults to comma.
     * @return True if successful, false otherwise.
     */
    inline bool append(const std::string& file_path, const std::vector<std::vector<std::string>>& data, char delimiter = ',') {
        std::filesystem::path path(file_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(file_path, std::ios::app); 
        if (!file.is_open()) {
            std::cerr << "Error: Could not create or open file for appending " << file_path << "\n";
            return false;
        }

        if (file.tellp() > 0) {
            file << "\n";
        }

        for (size_t i = 0; i < data.size(); ++i) {
            for (size_t j = 0; j < data[i].size(); ++j) {
                file << data[i][j];
                if (j < data[i].size() - 1) {
                    file << delimiter;
                }
            }
            if (i < data.size() - 1) {
                file << "\n";
            }
        }
        return true;
    }

    /**
     * @brief Counts the number of lines in a file.
     * @param file_path The full path to the file.
     * @return The total number of lines, or 0 if it cannot be opened.
     */
    inline int count_lines(const std::string& file_path) {
        if (!std::filesystem::exists(file_path)) {
            return 0;
        }

        std::ifstream file(file_path);
        if (!file.is_open()) {
            return 0;
        }

        int count = 0;
        std::string line;
        while (std::getline(file, line)) {
            count++;
        }
        return count;
    }

} namespace csv = file_manager::csv; // Alias for easier access

namespace convert {
    
    /**
     * @brief Converts a value of any type to its string representation.
     * @details Uses `std::ostringstream` for conversion, applying fixed-point notation with specified precision for floating-point types.
     * @tparam T The data type of the value to be converted.
     * @param value The value to convert.
     * @param precision The number of decimal places for floating-point types.
     * @return A string representation of the input value.
     */
    template <typename T>
    inline std::string to_string(const T& value, int precision = 6) {
        std::ostringstream out;
        if constexpr (std::is_floating_point_v<T>) {
            out << std::fixed << std::setprecision(precision);
        }
        out << value;
        return out.str();
    }

    /**
     * @brief Calculates a percentage and converts it to a formatted string.
     * @details Computes `(numerator / denominator) * 100.0` and returns it as a string. Safely handles division by zero.
     * @param numerator The value representing the part of the total.
     * @param denominator The value representing the total.
     * @param precision The number of decimal places for the resulting percentage string.
     * @return A string representation of the calculated percentage.
     */
    inline std::string to_percentage_string(double numerator, double denominator, int precision = 4) {
        if (denominator == 0.0) {
            return to_string(0.0, precision);
        }
        double percentage = (numerator / denominator) * 100.0;
        return to_string(percentage, precision);
    }

    /**
     * @brief Converts a vector of any primitive type to a JSON-like string representation.
     * @tparam T The type of elements in the vector.
     * @param vec The vector to convert.
     * @return A string formatted as an array (e.g., "[e1,e2,...]").
     */
    template <typename T>
    inline std::string vector_to_string(const std::vector<T>& vec) {
        if (vec.empty()) return "[]";
        std::string str = "[";
        for (size_t i = 0; i < vec.size(); ++i) {
            str += std::to_string(vec[i]);
            if (i < vec.size() - 1) str += ",";
        }
        str += "]";
        return str;
    }

    /**
     * @brief Converts a set of any primitive type to a JSON-like string representation.
     * @tparam T The type of elements in the set.
     * @param s The set to convert.
     * @return A string formatted as an array (e.g., "[e1,e2,...]").
     */
    template <typename T>
    inline std::string set_to_string(const std::set<T>& s) {
        if (s.empty()) return "[]";
        std::string str = "[";
        auto it = s.begin();
        while (it != s.end()) {
            str += std::to_string(*it);
            ++it;
            if (it != s.end()) str += ",";
        }
        str += "]";
        return str;
    }
}

namespace randomness {

    /**
     * @brief Provides a singleton instance of a high-quality random number engine.
     * @details Returns a reference to a static `std::mt19937` engine seeded with `std::random_device`.
     * @return A reference to the static `std::mt19937` random engine.
     */
    inline std::mt19937& get_engine() {
        static std::random_device rd;
        static std::mt19937 engine(rd());
        return engine;
    }

    /**
     * @brief Generates a random number within a specified inclusive interval.
     * @tparam T The numeric type (must be arithmetic).
     * @param min The lower bound of the interval (inclusive).
     * @param max The upper bound of the interval (inclusive).
     * @return A random number within the specified range.
     * @throws std::invalid_argument If min is greater than max.
     */
    template<typename T>
    inline T random_number(T min, T max) {
        static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type.");
        if (min > max) {
            throw std::invalid_argument("Error in random_number: min cannot be greater than max.");
        }
        if constexpr (std::is_integral_v<T>) {
            std::uniform_int_distribution<T> distribution(min, max);
            return distribution(get_engine());
        } else {
            std::uniform_real_distribution<T> distribution(min, max);
            return distribution(get_engine());
        }
    }

     /**
     * @brief Creates a vector of unique integers in random order within an inclusive range.
     * @tparam T The integral type for the range.
     * @param min The lower bound of the range (inclusive).
     * @param max The upper bound of the range (inclusive).
     * @return A randomly shuffled vector containing all integers from min to max.
     * @throws std::invalid_argument If min is greater than max.
     */
    template<typename T>
    inline std::vector<T> shuffled_range(T min, T max) {
        static_assert(std::is_integral_v<T>, "shuffled_range requires an integral type.");
        if (min > max) {
            throw std::invalid_argument("Error in shuffled_range: min cannot be greater than max.");
        }
        std::vector<T> numbers(max - min + 1);
        std::iota(numbers.begin(), numbers.end(), min);
        std::shuffle(numbers.begin(), numbers.end(), get_engine());
        return numbers;
    }
}

namespace structure {

    /**
     * @brief Slices a flat vector into a matrix (vector of vectors) based on a schedule.
     * @details Position [0] represents an empty step. Subsequent vectors contain sequential slices of the source.
     * @tparam T The type of elements in the source vector.
     * @param source The flat vector containing all elements to be distributed.
     * @param schedule A vector of integers where each value represents the number of elements for that step.
     * @return A matrix representing the data distributed over time.
     */
    template <typename T>
    inline std::vector<std::vector<T>> slice_by_schedule(const std::vector<T>& source, const std::vector<int>& schedule) {
        std::vector<std::vector<T>> matrix;
        matrix.reserve(schedule.size() + 1);

        // t = 0: Nothing happens (Empty vector)
        matrix.push_back(source);

        auto it = source.begin();
        for (int count : schedule) {
            std::vector<T> batch;
            batch.reserve(count);

            // Copy 'count' elements or until source runs out
            for (int i = 0; i < count && it != source.end(); ++i) {
                batch.push_back(*it);
                ++it;
            }
            
            matrix.push_back(std::move(batch));
        }

        return matrix;
    }

    /**
     * @brief Generates an increasing sequence and slices it based on a schedule.
     * @tparam T The numeric type for the sequence (e.g., int, long).
     * @param start The starting value of the sequence.
     * @param end The ending value of the sequence (inclusive).
     * @param schedule A vector of integers representing the sizes of each slice.
     * @return A matrix representing the sliced sequence over time.
     */
    template <typename T>
    inline std::vector<std::vector<T>> slice_by_schedule(T start, T end, const std::vector<int>& schedule) {
        std::vector<T> generated_source;

        if (start <= end) {
            size_t sequence_size = static_cast<size_t>(end - start + 1);
            generated_source.resize(sequence_size);
            std::iota(generated_source.begin(), generated_source.end(), start);
        }

        return slice_by_schedule(generated_source, schedule);
    }

    /**
     * @brief Appends the contents of the source vector to the destination vector.
     * @tparam T The type of elements in the vectors.
     * @param dest The destination vector to be modified.
     * @param src The source vector to be appended.
     */
    template <typename T>
    inline void append_vector(std::vector<T>& dest, const std::vector<T>& src) {
        dest.insert(dest.end(), src.begin(), src.end());
    }

    /**
     * @brief Merges two vectors into a new single vector.
     * @tparam T The type of elements in the vectors.
     * @param v1 The first vector.
     * @param v2 The second vector.
     * @return A new vector containing elements from v1 followed by v2.
     */
    template <typename T>
    inline std::vector<T> merge_vectors(const std::vector<T>& v1, const std::vector<T>& v2) {
        std::vector<T> result;
        result.reserve(v1.size() + v2.size());
        result.insert(result.end(), v1.begin(), v1.end());
        result.insert(result.end(), v2.begin(), v2.end());
        return result;
    }

    /**
     * @brief Merges two sets into a new single set.
     * @tparam T The type of elements in the sets.
     * @param s1 The first set.
     * @param s2 The second set.
     * @return A new set containing unique elements from both s1 and s2.
     */
    template <typename T>
    inline std::set<T> merge_sets(const std::set<T>& s1, const std::set<T>& s2) {
        std::set<T> result = s1;
        result.insert(s2.begin(), s2.end());
        return result;
    }
}

namespace sorting {

    /**
     * @brief Internal details for generic implementations.
     */
    namespace details {

        /**
         * @brief Internal sorting implementation logic that maps a member attribute.
         * @tparam Ascending Sort direction flag.
         * @tparam T Entity struct/class type.
         * @tparam AttributeType Data type of the sorted attribute.
         * @tparam AttributePtr Pointer-to-member specifying the comparison attribute.
         * @param entities Constant reference to the data vector.
         * @param idxs Vector of indices to be sorted.
         */
        template <bool Ascending, typename T, typename AttributeType, AttributeType T::* AttributePtr>
        void sort_indices_impl(const std::vector<T>& entities, std::vector<int>& idxs) {
            std::sort(idxs.begin(), idxs.end(), [&](int i, int j) {
                const auto& attr_i = entities.at(i).*AttributePtr;
                const auto& attr_j = entities.at(j).*AttributePtr;

                if (attr_i != attr_j) {
                    if constexpr (Ascending) {
                        return attr_i < attr_j; // Ascending order
                    } else {
                        return attr_i > attr_j; // Descending order
                    }
                }
                return i < j; // Tie-breaker
            });
        }
    }

    /**
     * @brief Sorts the indices of all entities (from 1 to N-1) by a specific struct/class attribute.
     * @details Original vector is not modified. Assumes 1-based indexing for internal simulation logic. 
     * @tparam Ascending If true, sorts in ascending order; otherwise, descending.
     * @tparam T The type of the entity.
     * @tparam AttributeType The data type of the member attribute.
     * @tparam AttributePtr Pointer-to-member specifying the attribute for comparison.
     * @param entities The constant vector of entities.
     * @return A new vector containing the sorted indices.
     */
    template <bool Ascending, typename T, typename AttributeType, AttributeType T::* AttributePtr>
    inline std::vector<int> sort_entities(const std::vector<T>& entities) {
        if (entities.size() <= 1) return {};
        std::vector<int> idxs(entities.size() - 1);
        std::iota(idxs.begin(), idxs.end(), 1); // Assumes 1-based indexing
        details::sort_indices_impl<Ascending, T, AttributeType, AttributePtr>(entities, idxs);
        return idxs;
    }

    /**
     * @brief Sorts a subset of entity indices by a specific attribute.
     * @tparam Ascending If true, sorts in ascending order; otherwise, descending.
     * @tparam T The type of the entity.
     * @tparam AttributeType The data type of the member attribute.
     * @tparam AttributePtr Pointer-to-member specifying the attribute for comparison.
     * @param entities The constant vector of entities.
     * @param indices_to_sort A vector of integers representing the subset of indices to sort.
     * @return A new vector containing the sorted subset indices.
     */
    template <bool Ascending, typename T, typename AttributeType, AttributeType T::* AttributePtr>
    inline std::vector<int> sort_entities(const std::vector<T>& entities, const std::vector<int>& indices_to_sort) {
        if (entities.empty() || indices_to_sort.empty()) return {};
        std::vector<int> idxs = indices_to_sort; // Create a copy to sort
        details::sort_indices_impl<Ascending, T, AttributeType, AttributePtr>(entities, idxs);
        return idxs;
    }
}