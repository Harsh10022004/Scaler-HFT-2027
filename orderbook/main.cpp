#include "order_book.h"
#include <iostream>

uint64_t get_ns() {
    static uint64_t ts = 0;
    return ++ts;
}

int main() {
    OrderBook book;

    std::cout << "--- Test 1: Initial Book State ---" << std::endl;
    book.add_order({101, true, 100.0, 50, get_ns()});
    book.add_order({102, true, 100.0, 25, get_ns()});
    book.add_order({103, true, 99.0,  100, get_ns()});
    book.add_order({104, false, 101.0, 75, get_ns()});
    book.add_order({105, false, 102.0, 50, get_ns()});
    book.add_order({106, false, 101.0, 10, get_ns()});
    book.print_book(5);


    std::cout << "\n--- Test 2: Cancel Order (102) ---" << std::endl;
    book.cancel_order(102);
    book.print_book(5);


    std::cout << "\n--- Test 3: Amend Quantity (103) ---" << std::endl;
    book.amend_order(103, 99.0, 120);
    book.print_book(5);


    std::cout << "\n--- Test 4: Amend Price (105) ---" << std::endl;
    book.amend_order(105, 100.5, 50);
    book.print_book(5);


    std::cout << "\n--- Test 5: Matching (Cross the book) ---" << std::endl;
    book.add_order({201, true, 102.0, 200, get_ns()});
    
    std::cout << "\n--- Test 6: Final Book State after Matching ---" << std::endl;
    book.print_book(5);
    
    std::cout << "\n--- Test 7: Matching (Partial Fill) ---" << std::endl;
    book.add_order({301, false, 99.5, 30, get_ns()});
    book.print_book(5);
    
    return 0;
}