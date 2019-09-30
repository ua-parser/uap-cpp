The snippet index adds a preprocessing layer that reduces the number of expressions to loop over, to save processing time. The way expressions are matched does not change, and still uses the `RE2` classes with the same expressions.

The idea for the preprocessing layer is that the expressions contain mandatory snippets of text that need to be present for the expression to match, and these snippets can be indexed in a tree structure that can be used to get the expressions that can possibly match for the input string in question.

```C++
SnippetIndex index;
SnippetMapping<int> mapping;

// Register expression 1 (foo.bar)
{
  auto snippetIds = index.registerSnippets("foo.bar");

  std::cout << "Expression 1 (foo.bar) needs" << std::endl;
  for (auto snippetId : snippetIds) {
    std::cout << " - snippet " << snippetId << std::endl;
  }

  mapping.addMapping(snippetIds, 1);
}

// Register expression 2 (foodbar)
{
  auto snippetIds = index.registerSnippets("foodbar");

  std::cout << "Expression 2 (foodbar) needs" << std::endl;
  for (auto snippetId : snippetIds) {
    std::cout << " - snippet " << snippetId << std::endl;
  }

  mapping.addMapping(snippetIds, 2);
}

// Match input strings
for (auto& inputString : {"barfood", "foobar", "foodbar", "foo"}) {
  std::cout << "Look up " << inputString << std::endl;

  auto snippetIds = index.getSnippets(inputString);
  for (auto snippetId : snippetIds) {
    std::cout << " - Found snippet " << snippetId << std::endl;
  }

  std::unordered_set<int> expressions;
  mapping.getExpressions(snippetIds, expressions);
  if (expressions.empty()) {
    std::cout << " No expression has all the snippet it needs" << std::endl;
  } else {
    for (int expressionId : expressions) {
      std::cout << " => Expression " << expressionId
                << " has all the snippets it needs" << std::endl;
    }
  }
}
```

```
Expression 1 (foo.bar) needs
 - snippet 1
 - snippet 2

Expression 2 (foodbar) needs
 - snippet 3

Look up barfood
 - Found snippet 1
 - Found snippet 2
 => Expression 1 has all the snippets it needs

Look up foobar
 - Found snippet 1
 - Found snippet 2
 => Expression 1 has all the snippets it needs

Look up foodbar
 - Found snippet 1
 - Found snippet 2
 - Found snippet 3
 => Expression 2 has all the snippets it needs
 => Expression 1 has all the snippets it needs

Look up foo
 - Found snippet 1
 No expression has all the snippet it needs
```

Snippet 1 is `foo`, snippet 2 `bar` and snippet 3 `foodbar`. The numbers are assigned in the order the snippets were first found when the expressions were registered.

For simplicity and lookup performance, the order of the snippets in the input does not matter. Remember that this is not a matter of exact matching, but a filter to avoid running unnecessary regular expression matchings.
