//
// Created by Mike on 5/17/2017.
//
#include "basic_bitmap.h"
#include "integral_image.h"
#include "weak_classifier.h"
#include "util/generate_weak_classifiers.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <set>

using namespace mcg;

template <typename func_t>
void test_runner(std::string&& m_name, func_t&& m_func)
{
    std::cout << "Running " << m_name << "... ";
    auto start = std::chrono::system_clock::now();
    m_func();
    using nanoseconds = std::chrono::nanoseconds;
    nanoseconds end = std::chrono::system_clock::now() - start;
    std::cout << "finished " << m_name << " in " << std::chrono::duration_cast<std::chrono::seconds>(end).count() << "s.\n";
}

#define MCG_RUN_TEST(test_name) \
    test_runner(#test_name, test_name);

void test_generate_weak_classifiers_kind_5()
{
    using wc_t = weak_classifier<integral_image<bitmap8>, 3, 3>;
    using vec_t = std::vector<wc_t>;
    vec_t vec;
    using iterator_t = decltype(std::back_inserter(vec));
    generate_features_<wc_t, haar_feature_kinds::kind_5, iterator_t>::run(std::back_inserter(vec));
    assert(vec.size() == 16); // Obtained from exhaustive counting.
}

void test_generate_symmetric_weak_classifiers_2()
{
    // Since the feature types are symmetric, the count should be the same;
    using wc_t = weak_classifier<integral_image<bitmap8>, 4, 4>;
    using vec_t = std::vector<wc_t>;
    vec_t vec1, vec2;
    using iterator_t = decltype(std::back_inserter(vec1));
    generate_features_<wc_t, haar_feature_kinds::kind_3, iterator_t>::run(std::back_inserter(vec1));
    generate_features_<wc_t, haar_feature_kinds::kind_4, iterator_t>::run(std::back_inserter(vec2));
    assert(vec1.size() == vec2.size());
}

void test_generate_symmetric_weak_classifiers_1()
{
    // Since the feature types are symmetric, the count should be the same;
    using wc_t = weak_classifier<integral_image<bitmap8>, 4, 4>;
    using vec_t = std::vector<wc_t>;
    vec_t vec1, vec2;
    using iterator_t = decltype(std::back_inserter(vec1));
    generate_features_<wc_t, haar_feature_kinds::kind_1, iterator_t>::run(std::back_inserter(vec1));
    generate_features_<wc_t, haar_feature_kinds::kind_2, iterator_t>::run(std::back_inserter(vec2));
    assert(vec1.size() == vec2.size());
}

void test_generate_weak_classifiers()
{
    using wc_t = weak_classifier<integral_image<bitmap8>, 24, 24>;
    using vec_t = std::vector<wc_t>;
    vec_t vec;
    using iterator_t = decltype(std::back_inserter(vec));
    generate_features_<wc_t, haar_feature_kinds::kind_1, iterator_t>::run(std::back_inserter(vec));
    const auto num_classifiers = 690000;
    assert(vec.size() == num_classifiers);
}

void test_next_offsets()
{
    std::array<size_t, 3> arr {1, 2, 3};
    std::set<std::tuple<size_t, size_t, size_t>> counter;
    for (auto i = 0; i < 40; ++i)
    {
        counter.insert(std::make_tuple(
                arr[0],
                arr[1],
                arr[2]));
        detail_gwc::next_limits(arr, 5);
    }
    assert(counter.size() == 20);

    std::array<size_t, 1> trivial {0};
    for (auto i = 0; i < 1000; ++i)
    {
        assert(trivial[0] == (i % 101));
        detail_gwc::next_limits(trivial, 100);
    }
}

void test_weak_classifier_with_bioid_dataset()
{
    static constexpr auto FILE_MAX = 152;
    using ii_t = integral_image<bitmap8>;
    std::vector<std::pair<ii_t, std::array<rect, 2>>> files ;
    std::cout << "Reading files...\n";
    static constexpr std::size_t w = 12;
    for (auto i = 0; i < FILE_MAX; ++i)
    {
        std::stringstream sstr;
        sstr << "../bioid-faces/BioID_"
             << std::setw(4) << std::setfill('0') << i << ".pgm";
        std::ifstream in(sstr.str());
        assert(in);
        std::string tmp;
        int width, height, max_pixel_val;
        in >> tmp >> width >> height >> max_pixel_val;
        bitmap8 bm(width, height);
        bm.m_data.resize(width * height);
        for (auto y = 0; y < height; ++y)
        {
            for (auto x = 0; x < width; ++x)
            {
                const auto c = in.get();
                bm.at(x, y) = c;
            }
        }
        in.close();
        sstr.str("");
        sstr << "../bioid-faces/BioID_"
             << std::setw(4) << std::setfill('0') << i << ".eye";
        in.open(sstr.str());
        assert(in);
        std::string comment;
        std::getline(in, comment, '\n');
        int lx, ly, rx, ry;
        in >> lx >> ly >> rx >> ry;
        if (lx >= w && ly >= w && rx >= w && ry >= w) {
            rect left_eye {lx - w, ly - w, 2 * w, 2 * w},
                 right_eye{rx - w, ry - w, 2 * w, 2 * w};
            std::array<rect, 2> eyes{
                    std::move(left_eye),
                    std::move(right_eye)
            };
            files.push_back(std::make_pair(ii_t::create(bm), std::move(eyes)));
        } else {
            std::cerr << "Bad file " << i << "\n";
        }
    }
    haar_feature<ii_t, 24, 24> feature(
            std::vector<rect>{ rect{0, 0, w/2, w} },
            std::vector<rect>{ rect{w/2, 0, w/2, w} }
    );
    weak_classifier<ii_t, 24, 24> wc{std::move(feature)};
    using data_t = typename weak_classifier<ii_t, 24, 24>::data_t;
    data_t pos, neg;
    std::cout << "Preparing data...\n";
    for (const auto& pair : files)
    {
        for (const auto& eye : pair.second)
        {
            pos.push_back(pair.first.vectorize_window(eye));
        }
        neg.push_back(pair.first.vectorize_window(rect{0, 0, 24, 24}));
    }
    std::cout << "Using " << pos.size() << " pos. samples\n"
              << "\tand " << neg.size() << " neg. samples\n";
    std::cout << "Training...\n";
    wc.train(pos, neg);
    std::cout << "threshold: " << wc.threshold << ", " << (wc.parity ? "x < threshold" : "x > threshold")<< "\n";
}

void test_weak_classifier_sanity_2()
{
    const int size = 100;
    bitmap8 bm(size, size);
    bm.m_data.resize(size*size);

    const auto mid = bm.m_data.begin() + (bm.m_data.size() / 2);
    std::fill(bm.m_data.begin(), mid, 0);
    std::fill(mid, bm.m_data.end(), 255);
    auto ii = integral_image<bitmap8>::create(bm);

    using ii_t = decltype(ii);
    haar_feature<ii_t, 24, 24> feature(std::vector<rect>{ rect{0, 0, 2, 2} },
                                       std::vector<rect>{ rect{2, 0, 2, 2} });
    weak_classifier<ii_t, 24, 24> w {std::move(feature)};

    using data_t = typename weak_classifier<ii_t, 24, 24>::data_t;
    const auto pos = ii.vectorize_window(rect {0, 0, 24, 24});
    data_t posv(10, pos);
    const auto neg = ii.vectorize_window(rect {76, 76, 24, 24});
    data_t negv(10, pos);
    w.train(posv, negv);
    std::cout << w.threshold << ", " << w.parity << "\n";
    w.predict(ii, rect {0, 0, 24, 24});
}


void test_integral_image()
{
    const int size = 100;
    bitmap8 bm(size, size);
    bm.m_data.resize(size*size);
    std::fill(bm.m_data.begin(), bm.m_data.end(), 128);
    auto ii = integral_image<bitmap8>::create(bm);
    assert(ii.at(0, 0)               == 128 * 1);
    assert(ii.at(0, size - 1)        == 128 * size);
    assert(ii.at(size - 1, 0)        == 128 * size);
    assert(ii.at(size - 1, size - 1) == 128 * size * size);
    assert(ii.at(10, 23)             == 128 * 11 * 24);
}

void test_weak_classifier_sanity()
{
    const int size = 100;
    bitmap8 bm(size, size);
    bm.m_data.resize(size*size);
    std::fill(bm.m_data.begin(), bm.m_data.end(), 128);
    auto ii = integral_image<bitmap8>::create(bm);
    std::vector<rect> pos {
            rect {0, 0, 2, 2}
    };
    std::vector<rect> neg {
            rect {2, 2, 2, 2}
    };
    using ii_t = decltype(ii);
    haar_feature<ii_t, 24, 24> feature(std::move(pos), std::move(neg));
    using ii_t = decltype(ii);
    weak_classifier<ii_t, 24, 24> w;

    w.predict(ii, rect {0, 0, 24, 24});
}

void test_vectorize_subwindow()
{
    const int size = 100;
    bitmap8 bm(size, size);
    bm.m_data.resize(size * size);

    for (auto i = 0; i < bm.m_data.size(); ++i)
    {
        bm.m_data[i] = i;
    }

    auto ii = integral_image<bitmap8>::create(bm);
    rect window{10, 10, 10, 10};
    auto vectorized_window = ii.vectorize_window(window);
    auto it = vectorized_window.begin();
    for (auto y = 10; y < 20; ++y)
    {
        for (auto x = 10; x < 20; ++x)
        {
            assert(*it == ii.at(x, y));
            ++it;
        }
    }
}

void test_read_bmp()
{
    std::ifstream in("test.bmp");
    bitmap24 bm = from_file<bitmap24>(in);
    // This test assumes that 24-bit pixels are stored in BGR-order instead of RGB-order.
    assert(bm.at(0, 0) == std::make_tuple(0, 0, 255));
    assert(bm.at(1, 0) == std::make_tuple(0, 255, 0));
    assert(bm.at(2, 0) == std::make_tuple(255, 0, 0));
    assert(bm.at(3, 0) == std::make_tuple(255, 255, 255));
    assert(bm.at(7, 7) == std::make_tuple(0, 0, 0));
}


void test_create_bmp24()
{
    std::ifstream in("test.bmp");
    bitmap24 bm = from_file<bitmap24>(in);
    auto ii = integral_image<bitmap24>::create(bm);
    assert(ii.at(0, 0) == bm.at(0, 0));
}


int main()
{
    MCG_RUN_TEST(test_integral_image);
    MCG_RUN_TEST(test_weak_classifier_sanity);
    MCG_RUN_TEST(test_vectorize_subwindow);
    MCG_RUN_TEST(test_read_bmp);
    MCG_RUN_TEST(test_create_bmp24);
    MCG_RUN_TEST(test_weak_classifier_sanity_2);
    MCG_RUN_TEST(test_weak_classifier_with_bioid_dataset);
    MCG_RUN_TEST(test_next_offsets);
    MCG_RUN_TEST(test_generate_weak_classifiers);
    MCG_RUN_TEST(test_generate_symmetric_weak_classifiers_1);
    MCG_RUN_TEST(test_generate_symmetric_weak_classifiers_2);
    MCG_RUN_TEST(test_generate_weak_classifiers_kind_5);
    return 0;
}