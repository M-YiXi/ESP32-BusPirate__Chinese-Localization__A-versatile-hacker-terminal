#ifndef TEST_HORIZONTAL_SELECTORS_H
#define TEST_HORIZONTAL_SELECTORS_H

#include <unity.h>
#include "../src/Selectors/HorizontalSelector.h"
#include "../Views/MockView.h"
#include "../Inputs/MockInput.h"

void test_horizontal_selector_confirm() {
    MockView mockView;
    MockInput mockInput;
    InactivityManager manager(mockView);

    HorizontalSelector horizontalSelector(mockView, mockInput, manager);

    std::vector<std::string> options = {"OptionA", "OptionB", "OptionC"};
    std::string title = "Horizontal Selector Test";

    // Mock Input
    mockInput.enqueueKey(KEY_ARROW_RIGHT);
    mockInput.enqueueKey(KEY_ARROW_RIGHT);
    mockInput.enqueueKey(KEY_ARROW_LEFT);
    mockInput.enqueueKey(KEY_OK);

    int selectedIndex = horizontalSelector.select(title, options);

    TEST_ASSERT_EQUAL(1, selectedIndex); // La deuxième option est sélectionnée
    TEST_ASSERT_TRUE(mockView.topBarCalled);
    TEST_ASSERT_TRUE(mockView.horizontalSelectionCalled);
    TEST_ASSERT_EQUAL_STRING(title.c_str(), mockView.lastTitle.c_str());
    TEST_ASSERT_EQUAL_STRING("OptionB", mockView.displayedOptions[selectedIndex].c_str());
}

void test_horizontal_selector_cancel() {
    MockView mockView;
    MockInput mockInput;
    InactivityManager manager(mockView);

    HorizontalSelector horizontalSelector(mockView, mockInput, manager);

    std::vector<std::string> options = {"OptionA", "OptionB", "OptionC"};
    std::string title = "Horizontal Selector Test";

    // Mock Input
    mockInput.enqueueKey(KEY_ESC_CUSTOM);

    int selectedIndex = horizontalSelector.select(title, options);

    TEST_ASSERT_EQUAL(-1, selectedIndex); // Retourne -1 si l'utilisateur appuie sur RETURN
}

#endif // TEST_HORIZONTAL_SELECTORS_H
