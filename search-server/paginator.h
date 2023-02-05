#pragma once
#include <iostream>

template <typename Iterator>
struct IteratorRange {
    Iterator begin;
    Iterator end;
    IteratorRange(Iterator begin, Iterator end) :begin(begin), end(end) {}
};

template <typename Iterator>
class Paginator {
public:

    Paginator(Iterator begin, Iterator end, int size)
        :page_size_(size) {
        vector test(begin, end);
        Iterator temp = begin;
        for (; temp + size < end; temp += size) {
            pages_.push_back(IteratorRange(temp, temp + size));
        }
        if (temp < end) {
            pages_.push_back(IteratorRange(temp, end));
        }
    }

    auto begin() const {
        return pages_.begin();
    }
    auto end() const {
        return pages_.end();
    }
    int size() const {
        return page_size_;
    }

private:
    int page_size_;
    vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template<typename Iterator>
ostream& operator<< (ostream& out, IteratorRange<Iterator> p) {
    for (auto i = p.begin; i < p.end; i++) {
        out << *i;
    }
    return out;
}
