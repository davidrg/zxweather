from twisted.internet.protocol import Protocol, connectionDone


class Event(object):
    """
    A basic event system. Looks kind of like the .net one:

    foo.event += bar.handler
    foo.event -= bar.handler
      or:
    foo.event.clearHandlers(bar)

    """
    def __init__(self):
        self._handlers = []

    def __iadd__(self, other):
        self._handlers.append(other)
        return self

    def __isub__(self, other):
        self._handlers.remove(other)
        return self

    def fire(self, *args, **kwargs):
        """
        Fires the event.
        :param args: Arguments
        :param kwargs: More arguments
        """
        for handler in self._handlers:
            handler(*args, **kwargs)

    def removeHandlers(self, parent_object):
        """
        Clears all handlers that belong to the supplied parent object.
        :param parent_object: The object to clear handlers for
        """

        # We can't remove handlers in the loop so we'll add them to this and
        # remove them all once we've identified what we're removing.
        to_remove = []

        for handler in self._handlers:
            if hasattr(handler, 'im_self'):
                if handler.im_self == parent_object:
                    to_remove.append(handler)

        for handler in to_remove:
            self.__isub__(handler)
