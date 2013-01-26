# coding=utf-8
"""
Misc utility stuff for testing.
"""
__author__ = 'david'

class CallTracker(object):
    """
    Used to see if a function gets called. Wrap the function using this class
    and then check the called member to see if it was called. Parameters will
    be passed on and the return value will be returned.

    >>> def foo(param):
    ...     print("Hello, " + param)
    ...     return 42
    ...
    >>> bar = CallTracker(foo)
    >>> result = bar("World!")
    Hello, World!
    >>> result
    42
    """

    def __init__(self, real_function):
        self.called = False
        self._real_function = real_function

    def __call__(self, *args, **kwargs):
        self.called = True
        return self._real_function(*args, **kwargs)



