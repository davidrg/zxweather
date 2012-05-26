# coding=utf-8
"""
This module contains various utility functions for doing things such as
computing relative URLs.
"""
__author__ = 'David Goodwin'

def is_not_empty(val):
    """
    Returns true if the string is not equal to ''
    :param val: String to check
    :type val: str
    :return: False if the string is ''
    """
    if val == '':
        return False
    return True

def path_split(path):
    """
    Splits the supplied path on the '/' character. Any empty segments are
    discarded.
    :param path: The path to split.
    :type path: str
    :return: A list containing all the non-empty components of the path.
    :rtype: []
    """
    components = path.split('/')
    components = filter(is_not_empty,components)
    return components

def path_join(path):
    """
    Joins a list of path segments without introducing duplicate separators.
    :param path: list of path components
    :type path: []
    :return: Joined path
    :rtype: str
    """
    new_path = ""
    for part in path:
        if new_path.endswith("/") or new_path == '':
            new_path += part
        else:
            new_path += '/' + part

    return new_path

def common_path(path1, path2, common=None):
    """
    Finds the list of common components to the two supplied paths and returns
    the two paths minus the common components.
    :param path1: First path
    :type path1: []
    :param path2: Second path
    :type path2: []
    :param common: List of common components found so far. Leave as None.
    :type common: []
    :return: A list of components common to both paths, a list of components
             unique to path1, a list of components unique to path2.
    :rtype: [],[],[]
    """
    if common is None: common = []

    if len(path1) < 1: return common, path1, path2
    if len(path2) < 1: return common, path1, path2
    if path1[0] != path2[0]: return common, path1, path2

    return common_path(path1[1:], path2[1:], common+[path1[0]])

def relative_url(current_url, target_url):
    """
    Computes a relative URL to get from current_url to target_url.
    :param current_url: The URL to come from
    :type current_url: str
    :param target_url: The URL to go to
    :type target_url: str
    :return: A version of target_url relative to current_url.
    :rtype: str
    """
    common, path1, path2 = common_path(path_split(current_url), path_split(target_url))

    depth = len(path1)

    # If current_url does not end with a '/' then the final entry in path1
    # is actually a file, not a directory. We have to account for this in the
    # depth calculation.
    if not current_url.endswith("/"):
        depth -= 1

    p = []
    if depth > 0:
        p = ['../' * depth]
    p = p + path2

    path = path_join(p)

    # If the target URL ends with a '/' then whatever we return must end with
    # one too.
    if path == "" and not current_url.endswith("/") and target_url.endswith("/"):
        path = "./" # Up to current directory
    elif path == "":
        path = "#" # Stay on current page
    elif target_url.endswith('/') and not path.endswith("/"):
        path += '/' # Target is a directory so relative URL must be one too.

    return path
