import psycopg2.extensions

from ui import get_boolean


def retry_failed_images(con: psycopg2.extensions.connection):
    print("""
Retry Failed Images
-------------------

When weather-push encounters an error trying to send an image to a remote site
it will wait 10 minutes and try to send the image again. It will make up to
four retries for a total of five transmit attempts. 

After five failed transmit attempts the image is ignored so as to not waste 
bandwidth in situations where the failure is not recoverable (such as a bug in
the weather-push client or server).

This utility allows you to reset the retry count on all failed images allowing
them to be reconsidered for transmission to remote sites. Once reset images
will be transmitted on the standard schedule (up to 5 images sent each time a 
new image is stored in the database).

The following remote sites have failed images:

id  hostname                                      count
--- --------------------------------------------- -----""")

    query = """
select rs.site_id, rs.hostname, count(*) as count
from image_replication_status irs
inner join remote_site rs on rs.site_id = irs.site_id
where retries >= 5
  and irs.status in ('pending', 'awaiting_confirmation')
group by rs.site_id, rs.hostname
    """

    cur = con.cursor()
    cur.execute(query)
    results = cur.fetchall()
    for result in results:
        print("{0: <3} {1: <45} {2: <5}".format(*result))

    print("\n")

    proceed = get_boolean("Reset retry count on all failed images?", False)

    if proceed:
        print("Resetting retry counts...")
        query = """
update image_replication_status set retries = 0 where status in ('pending', 'awaiting_confirmation');
        """
        cur.execute(query)
        con.commit()
        print("Retry counts reset. ")
    else:
        print("** User Canceled")
