# coding=utf-8
__author__ = 'david'

_session_tracking_sessions = {}

_session_tracking_total_sessions = 0

def register_session(sid, data=None):
    """
    Registers a new session
    :param sid: Session ID
    :param data: Any data to be stored about the session (client info, etc)
    """
    global _session_tracking_total_sessions, _session_tracking_sessions

    if data is None:
        data = {}

    _session_tracking_sessions[sid] = data
    _session_tracking_total_sessions += 1

def update_session(sid, key, data):
    """
    Updates the data stored about a session.
    :param sid: Session ID
    :param key: Key to store data against
    :param data: New session data.
    """
    global _session_tracking_sessions
    _session_tracking_sessions[sid][key] = data

def get_session_value(sid, key):
    """
    Returns information from the session.
    :param sid: Session ID
    :param key: Key to look up
    """
    global _session_tracking_sessions

    if key not in _session_tracking_sessions[sid]:
        return None

    return _session_tracking_sessions[sid][key]

def end_session(sid):
    """
    Removes session information.
    :param sid: Session ID
    :return:
    """
    global _session_tracking_sessions
    _session_tracking_sessions.pop(sid)

def get_session_counts():
    """
    Gets session counts
    :return: Current sessions, Total sessions
    :rtype: int,int
    """
    global _session_tracking_total_sessions, _session_tracking_sessions

    return len(_session_tracking_sessions), _session_tracking_total_sessions

def get_session_id_list():
    """
    Returns a list of all active sessions
    :return: Active session list
    :rtype: list
    """
    global _session_tracking_sessions
    return _session_tracking_sessions.keys()

def session_exists(sid):
    """
    Checks to see if the specified session exists.
    :param sid: Session ID
    :return: True if it exists, False if it doesn't
    """
    global _session_tracking_sessions
    return sid in _session_tracking_sessions