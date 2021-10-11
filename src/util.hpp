#pragma once

#include "common.hpp"
#include <type_traits>

template <class T>
struct Node {
    T *prev, *next;

    Node() = default;
};

template <typename T>
struct List {
    T *front = nullptr, *back = nullptr;

    void push(T *u) {
        u->prev = back;
        u->next = nullptr;
        if (back) {
            back->next = u;
            back = u;
        } else
            front = back = u;
    }

    void push_front(T *u) {
        u->prev = nullptr;
        u->next = front;
        if (front) {
            front->prev = u;
            front = u;
        } else
            front = back = u;
    }

    void insert(T *u, T *before_u) {
        before_u->next = u;
        before_u->prev = u->prev;
        if (u->prev)
            u->prev->next = before_u;
        u->prev = before_u;
        if (front == u)
            front = before_u;
    }

    void erase(T *u) {
        if (u->prev)
            u->prev->next = u->next;
        else
            front = u->next;

        if (u->next)
            u->next->prev = u->prev;
        else
            back = u->prev;
    }

    bool empty() const {
        if (front == nullptr) {
            asserts(back == nullptr);
            return true;
        }
        asserts(back != nullptr);
        return false;
    }
};

#define FOR_LIST(T, o, l) for (T *o = (l).front; o; o = o->next)

template <typename T, typename U>
T *as_a(U *p) {
    static_assert(std::is_convertible<T *, U *>::value, "invalid as_a");
    return dynamic_cast<T *>(p);
}

template <typename T, typename U>
bool is_a(U *p) {
    static_assert(std::is_convertible<T *, U *>::value, "invalid as_a");
    return dynamic_cast<const T *>(p) != nullptr;
}

#define if_a(T, x, p) if (auto *x = dynamic_cast<T *>(p))
