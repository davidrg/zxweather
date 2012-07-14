# coding=utf-8
"""
Generates signing keys
"""
from optparse import OptionParser
import gnupg

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
                      help="File to save the public key as")

    options, args = parser.parse_args()

    return options

def main():
    """
    Does key stuff
    :return:
    """

    options = parse_args()

    gpg_home = options.gpg_home
    gpg_binary = options.gpg_binary
    pubkey_filename = options.pubkey_filename

    if gpg_home is None:
        print("ERROR: GnuPG Home directory for db_push tool must be supplied.")
        return

    if pubkey_filename is None:
        pubkey_filename = 'zxweather_public.asc'

    print("This tool generates signing keys for zxweather database replication.")
    print("GnuPG Home: {0}".format(gpg_home))
    print("Public Key Filename: {0}".format(pubkey_filename))

    print("Creating a new signing key. This may take a while.")

    if gpg_binary is not None:
        gpg = gnupg.GPG(gnupghome=gpg_home, gpgbinary=gpg_binary)
    else:
        gpg = gnupg.GPG(gnupghome=gpg_home)
    input_data = gpg.gen_key_input()
    key = gpg.gen_key(input_data)

    print("Key Fingerprint: " + str(key))
    with open(pubkey_filename, 'w') as f:
        f.write(gpg.export_keys(key))
    print("Public key (for web server) written to {0}".format(pubkey_filename))

if __name__ == "__main__": main()