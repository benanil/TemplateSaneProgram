
#include <stdio.h>

#include "ASTL/Algorithms.hpp"
#include "ASTL/Array.hpp"
#include "ASTL/IO.hpp"
#include "ASTL/RedBlackTree.hpp"

static int Day1_1()
{
    ScopedText data = ReadAllText("AdventInput1.txt");
    const char* curr = data;
    
    Array<int> leftList;
    Array<int> rightList;

    while (*curr) {
        leftList.Add(ParsePositiveNumber(curr));
        rightList.Add(ParsePositiveNumber(curr));
    }
    
    ShellSort(leftList.arr, leftList.Size());
    ShellSort(rightList.arr, leftList.Size());

    int result = 0;
    for (int i = 0; i < leftList.Size(); i++) {
        int diff = Abs(leftList[i] - rightList[i]);
        result += diff;
    }

    printf("result is: %d", result);
    getchar();

    return 0;
}

static int day1_2()
{
    ScopedText data = ReadAllText("AdventInput1.txt");
    const char* curr = data;
    
    Array<int> leftList{};
    Map<int, int> rightList{};

    while (*curr)
    {
        leftList.Add(ParsePositiveNumber(curr));
        
        int left = ParsePositiveNumber(curr);
        KeyValuePair keyval = KeyValuePair<int, int>(left, 1);
     
        if (auto* node = rightList.FindNode(keyval))
        {
            node->value.value += 1;
        }
        else
        {
            rightList.Insert(keyval);
        }
    }

    QuickSort(leftList.arr, 0, leftList.Size()-1);

    int result = 0;
    for (int i = 0; i < leftList.Size(); )
    {
        int number = leftList[i];
        auto* node = rightList.FindNode(KeyValuePair<int, int>(number, 1));
        
        if (node != nullptr) {
            result += number * node->value.value;
        }

        while (leftList[i] == number && i < leftList.Size()) {
            i++;
        }
    }
    printf("result is: %d", result);
    getchar();

    return 0;
}

static bool IsSafe(int* numbers, int n)
{
	int prev = numbers[0];
	bool isIncreasing = true;
	bool isDecreasing = true;
	bool anyEqual = false;
    
	for (int i = 1; i < n; i++) 
	{
		int num = numbers[i];
		isIncreasing &= num >= prev;
		isDecreasing &= num <= prev;
		anyEqual |= num == prev;

		if (Abs(num - prev) > 3) {
			isIncreasing = false;
			isDecreasing = false;
		}
		prev = num;
	}
    
	return (isIncreasing || isDecreasing) && !anyEqual;
}

int Day2()
{
	FILE* input = fopen("AdventInput2.txt", "r");
	char line[128];
	int numbers[12];
	int numbers2[12];
	int n = 0;
	int numSafe = 0;

	while (fgets(line, sizeof(line), input))
	{
		const char* curr = line;
		while (*curr > '\n')
		{
			int num;
			if (IsNumber(*curr)) num = *curr - '0', curr++;
			if (IsNumber(*curr)) num = num * 10 + *curr - '0', curr++;
			curr += *curr == ' ';
			curr += *curr == '\r';
			numbers[n++] = num;
		}
		
		if (IsSafe(numbers, n))
		{
			numSafe++;
		}
		#ifdef PART2
		else
		{
			int n2 = 0;
			for (int i = 0; i < n; i++)
			{
				for (int j = 0; j < n; j++)
					if (j != i) numbers2[n2++] = numbers[j];
				
				if (IsSafe(numbers2, n2)) {
					numSafe++;
					break;
				}
				n2 = 0;
			}
		}
		#endif
		n = 0;
	}
	printf("num safe: %d",  numSafe);
	fclose(input);
	getchar();
	return numSafe;
}

int main()
{
	ScopedText text = ReadAllText("AdventInput3.txt");
	const char* curr = text.text;
	long result = 0;
	bool enabled = true;

	while (*curr)
	{
		#if PART2
		if (StartsWith(curr, "don't()")) enabled = false;
		if (StartsWith(curr, "do()")) enabled = true;
		#endif
		
		if (enabled && StartsWith(curr, "mul("))
		{
			int a = -1, b = -1;

			if (IsNumber(*curr)) a = *curr++ - '0'; 
			if (IsNumber(*curr)) a = 10 * a + *curr++ - '0';
			if (IsNumber(*curr)) a = 10 * a + *curr++ - '0';
			
			if (a == -1 || *curr != ',')
				continue;
			curr++; // skip ','

			if (IsNumber(*curr)) b = *curr++ - '0';
			if (IsNumber(*curr)) b = 10 * b + *curr++ - '0';
			if (IsNumber(*curr)) b = 10 * b + *curr++ - '0';
		
			if (b == -1 || *curr != ')')
				continue;
			result += a * b;
		}

		curr += *curr != 0;
	}

	printf("result: %ld\n", result);
	getchar();
	return result;
}