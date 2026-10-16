/**
 * @file tools.hpp
 * @brief General-purpose utilities for CSV I/O, string formatting, random number generation, and indirect sorting.
 * @details Header-only toolkit providing foundational helper functions used across the MMKP simulator.
 */

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
 * @brief Small epsilon constant used for floating-point comparisons and non-zero lower bounds.
 */
constexpr double ZERO = 1e-13;

/**
 * @brief Global default precision (number of decimal places) for floating-point string formatting.
 */
constexpr int DECIMAL_PLACES = 10;

/**
 * @namespace file_manager::csv
 * @brief Provides file I/O operations for reading, writing, and appending delimited CSV/text files.
 */
namespace file_manager::csv {

    /**
     * @brief Reads a delimited CSV file into a 2D vector of strings.
     * @param file_path The full path to the file to be read.
     * @param delimiter The character that separates columns (defaults to `','`).
     * @return An `std::optional` containing the 2D vector of strings upon success, or `std::nullopt` on failure.
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
     * @brief Writes a 2D vector of strings to a CSV file, creating parent directories if needed.
     * @param file_path The full path to the output file.
     * @param data The 2D vector of strings to write.
     * @param delimiter The character used for separating columns (defaults to `','`).
     * @return `true` if the file was written successfully; `false` otherwise.
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
     * @brief Appends a 2D vector of strings to an existing or new CSV file.
     * @param file_path The full path to the output file.
     * @param data The 2D vector of strings to append.
     * @param delimiter The character used for separating columns (defaults to `','`).
     * @return `true` if the data was appended successfully; `false` otherwise.
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
     * @brief Counts the total number of lines in a file.
     * @param file_path The full path to the file.
     * @return The total number of lines, or `0` if the file does not exist or cannot be opened.
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

}

/**
 * @brief Short namespace alias for `file_manager::csv`.
 */
namespace csv = file_manager::csv;

/**
 * @namespace convert
 * @brief String conversion and formatting utilities for numbers, percentages, vectors, and sets.
 */
namespace convert {
    
    /**
     * @brief Converts a value of any streamable type to its string representation.
     * @details Uses `std::ostringstream` for conversion, applying fixed-point notation with the specified precision for floating-point types.
     * @tparam T The data type of the value to be converted.
     * @param value The value to convert.
     * @param precision The number of decimal places for floating-point types.
     * @return A formatted string representation of `value`.
     */
    template <typename T>
    inline std::string to_string(const T& value, int precision = DECIMAL_PLACES) {
        std::ostringstream out;
        if constexpr (std::is_floating_point_v<T>) {
            out << std::fixed << std::setprecision(precision);
        }
        out << value;
        return out.str();
    }

    /**
     * @brief Calculates a percentage and converts it to a formatted string.
     * @details Computes $(\text{numerator} / \text{denominator}) \times 100.0$ and formats it to `precision` decimal places. Safely returns `0.0` when `denominator == 0.0`.
     * @param numerator The value representing the part of the total.
     * @param denominator The value representing the total.
     * @param precision The number of decimal places for the resulting percentage string.
     * @return A string representation of the calculated percentage.
     */
    inline std::string to_percentage_string(double numerator, double denominator, int precision = DECIMAL_PLACES) {
        if (denominator == 0.0) {
            return to_string(0.0, precision);
        }
        double percentage = (numerator / denominator) * 100.0;
        return to_string(percentage, precision);
    }

    /**
     * @brief Converts a vector of primitive types to a bracketed array string representation.
     * @tparam T The type of elements in the vector.
     * @param vec The vector to convert.
     * @return A string formatted as `"[e1,e2,...]"`.
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
     * @brief Converts a `std::set` of primitive types to a bracketed array string representation.
     * @tparam T The type of elements in the set.
     * @param s The set to convert.
     * @return A string formatted as `"[e1,e2,...]"`.
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

/**
 * @namespace randomness
 * @brief Mersenne Twister (`std::mt19937`) random number generation and permutation utilities.
 */
namespace randomness {

    /**
     * @brief Provides a singleton instance of a pseudo-random number engine.
     * @details Returns a reference to a static `std::mt19937` engine seeded once via `std::random_device`.
     * @return A reference to the static `std::mt19937` random engine.
     */
    inline std::mt19937& get_engine() {
        static std::random_device rd;
        static std::mt19937 engine(rd());
        return engine;
    }

    /**
     * @brief Generates a uniform random number within a specified inclusive interval $[\text{min}, \text{max}]$.
     * @tparam T The numeric type (must satisfy `std::is_arithmetic_v<T>`).
     * @param min The lower bound of the interval (inclusive).
     * @param max The upper bound of the interval (inclusive).
     * @return A uniformly distributed random number within $[\text{min}, \text{max}]$.
     * @throws std::invalid_argument If `min > max`.
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
     * @brief Generates a random number within $[\text{min}, \text{max}]$, excluding values present in `exclude`.
     * @details The generated number is automatically appended to `exclude` to prevent future duplicates.
     * @tparam T The numeric type (must satisfy `std::is_arithmetic_v<T>`).
     * @param min The lower bound of the interval (inclusive).
     * @param max The upper bound of the interval (inclusive).
     * @param exclude A reference to a vector containing values to be excluded.
     * @return A random number within $[\text{min}, \text{max}]$ not previously in `exclude`.
     * @throws std::invalid_argument If `min > max`, or if all possible integral values in the range are already excluded.
     */
    template<typename T>
    inline T random_number(T min, T max, std::vector<T>& exclude) {
        static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type.");
        
        if (min > max) {
            throw std::invalid_argument("Error in random_number: min cannot be greater than max.");
        }
        
        if constexpr (std::is_integral_v<T>) {
            if (exclude.size() >= static_cast<size_t>(max - min + 1)) {
                throw std::invalid_argument("Error in random_number: all possible values are excluded causing infinite loop.");
            }
        }

        T result;
        if constexpr (std::is_integral_v<T>) {
            std::uniform_int_distribution<T> distribution(min, max);
            do {
                result = distribution(get_engine());
            } while (std::find(exclude.begin(), exclude.end(), result) != exclude.end());
        } else {
            std::uniform_real_distribution<T> distribution(min, max);
            do {
                result = distribution(get_engine());
            } while (std::find(exclude.begin(), exclude.end(), result) != exclude.end());
        }
        
        exclude.push_back(result);
        return result;
    }

    /**
     * @brief Creates a randomly shuffled vector containing all integers in the inclusive range $[\text{min}, \text{max}]$.
     * @tparam T The integral type for the range.
     * @param min The lower bound of the range (inclusive).
     * @param max The upper bound of the range (inclusive).
     * @return A randomly shuffled `std::vector<T>` of unique integers from `min` to `max`.
     * @throws std::invalid_argument If `min > max`.
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

/**
 * @namespace sorting
 * @brief Generic index-based sorting utilities for ordering 1-indexed entity vectors by member attributes.
 */
namespace sorting {

    /**
     * @namespace sorting::details
     * @brief Internal implementation helpers for pointer-to-member index sorting.
     */
    namespace details {

        /**
         * @brief Sorts a vector of indices based on a specified struct/class member attribute.
         * @tparam Ascending Sort direction flag (`true` for ascending, `false` for descending).
         * @tparam T Entity struct/class type.
         * @tparam AttributeType Data type of the sorted member attribute.
         * @tparam AttributePtr Pointer-to-member specifying the comparison attribute.
         * @param entities Constant reference to the entity vector.
         * @param idxs Vector of 1-based indices to be sorted in-place.
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
     * @brief Sorts the indices of all entities (from `1` to `N-1`) by a specific struct/class attribute.
     * @details Does not mutate the original `entities` vector. Assumes 1-based indexing where index `0` is a placeholder.
     * @tparam Ascending If `true`, sorts in ascending order; otherwise, descending.
     * @tparam T The type of the entity.
     * @tparam AttributeType The data type of the member attribute.
     * @tparam AttributePtr Pointer-to-member specifying the attribute for comparison.
     * @param entities The constant 1-indexed vector of entities.
     * @return A new `std::vector<int>` containing the sorted 1-based indices.
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
     * @brief Sorts a specific subset of entity indices by a struct/class member attribute.
     * @tparam Ascending If `true`, sorts in ascending order; otherwise, descending.
     * @tparam T The type of the entity.
     * @tparam AttributeType The data type of the member attribute.
     * @tparam AttributePtr Pointer-to-member specifying the attribute for comparison.
     * @param entities The constant 1-indexed vector of entities.
     * @param indices_to_sort A vector of integers representing the subset of 1-based indices to sort.
     * @return A new `std::vector<int>` containing the sorted subset indices.
     */
    template <bool Ascending, typename T, typename AttributeType, AttributeType T::* AttributePtr>
    inline std::vector<int> sort_entities(const std::vector<T>& entities, const std::vector<int>& indices_to_sort) {
        if (entities.empty() || indices_to_sort.empty()) return {};
        std::vector<int> idxs = indices_to_sort; // Create a copy to sort
        details::sort_indices_impl<Ascending, T, AttributeType, AttributePtr>(entities, idxs);
        return idxs;
    }
}