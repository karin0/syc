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

    T *push(T *u) {
        u->prev = back;
        u->next = nullptr;
        if (back) {
            back->next = u;
            back = u;
        } else
            front = back = u;
        return u;
    }

    T *push_front(T *u) {
        u->prev = nullptr;
        u->next = front;
        if (front) {
            front->prev = u;
            front = u;
        } else
            front = back = u;
        return u;
    }

    T *insert(T *u, T *before_u) {
        before_u->next = u;
        before_u->prev = u->prev;
        if (u->prev)
            u->prev->next = before_u;
        u->prev = before_u;
        if (front == u)
            front = before_u;
        return before_u;
    }

    T *insert_after(T *u, T *after_u) {
        after_u->prev = u;
        after_u->next = u->next;
        if (u->next)
            u->next->prev = after_u;
        u->next = after_u;
        if (back == u)
            back = after_u;
        return after_u;
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

    void replace(const T *o, T *n) {
        n->prev = o->prev;
        n->next = o->next;
        if (n->prev)
            n->prev->next = n;
        if (n->next)
            n->next->prev = n;
        if (front == o)
            front = n;
        if (back == o)
            back = n;
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

#define FOR_LIST(o, l) for (auto *o = (l).front; o; o = o->next)
#define FOR_LIST_MUT(o, l) for (decltype((l).front) o = (l).front, o##_next; o && ((o##_next = o->next), true); o = o##_next)

template <typename T, typename U>
T *as_a(U *p) {
    static_assert(std::is_convertible<T *, U *>::value, "invalid as_a");
    return dynamic_cast<T *>(p);
}

template <typename T, typename U>
bool is_a(U *p) {
    static_assert(std::is_convertible<T *, U *>::value, "invalid is_a");
    return dynamic_cast<const T *>(p) != nullptr;
}

#define if_a(T, x, p) if (auto *x = dynamic_cast<T *>(p))
