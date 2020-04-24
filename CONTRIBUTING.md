# FOnline Engine Contribution Guidelines

If you want to contribute to FOnline Engine, there are several ways how you can help out.

## Using the Engine

Just using the engine and filing bug reports, feature requests or generally giving feedbackiving feedback is a great way of contributing.

## Documentation

If you in aware of FOnline Engine API and know English then any contribution in documentation appricieted.  
You can find documentation stubs in `Source/Scripting/ScriptApi_*.h` files.

Check commented sections like this
```
/*******************************************************************************
 * ...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(Foo, FO_API_RET(void))
```

And fix to on something like
```
/*******************************************************************************
 * This function makes your life easier. And something else here. And here is a
 * line break...
 ******************************************************************************/
FO_API_GLOBAL_COMMON_FUNC(Foo, FO_API_RET(void))
```

Then create Pull Request.  
All changes in this documentation automatically reflects to all scripting layers and markdown documentation.

## Bugfixes

If you find a bug, and manage to fix it then create a Pull Request.  
Don't forget following code style and make code formatting before request.

## Features

If you feel confident enough to add a whole new feature then contact with me by email `cvet@tut.by` and we discuss the best way of how we can achieve it.  
As start point of needed features and bugfixes you can check at [Work in progress section in main readme](https://fonline.ru/#work-in-progress).
