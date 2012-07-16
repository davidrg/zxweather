# coding=utf-8
"""
This tool installs a public key for zxweather database replication. There
isn't anything at all special about this program - gnupg can do this all
itself if you read the manual to find out how.
"""
from optparse import OptionParser
import gnupg

__author__ = 'David Goodwin'

def parse_args():
    """
    Parses args.
    :return:
    """

    # Configure and run the option parser
    parser = OptionParser()
    parser.add_option("-d", "--gpg-home-directory", dest="gpg_home",
                      help="GnuPG Home Directory for db_push tool")
    parser.add_option("-b","--gpg-binary",dest="gpg_binary",
                      help="Name of GnuPG Binary")
    parser.add_option("-f","--pubkey-filename",dest="pubkey_filename",
                      help="Public key to import")

    options, args = parser.parse_args()
    return options

def main():
    """
    Program entry point
    :return:
    """

    options = parse_args()

    gpg_home = options.gpg_home
    gpg_binary = options.gpg_binary
    pubkey_filename = options.pubkey_filename

    if gpg_binary is None:
        gpg = gnupg.GPG(gnupghome = options.gpg_home)
    else:
        gpg = gnupg.GPG(gnupghome= gpg_home, gpgbinary = gpg_binary)

    key_data = open(pubkey_filename).read()
    import_result = gpg.import_keys(key_data)
    if len(import_result.fingerprints) > 0:
        print("\nKey Fingerprint: {0}".format(import_result.fingerprints[0]))
        print("Add the fingerprint above to the web interface configuration file")
    else:
        print("ERROR: No keys imported")

if __name__ == "__main__": main()