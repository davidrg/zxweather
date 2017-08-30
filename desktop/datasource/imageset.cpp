#include "imageset.h"
#include "settings.h"


bool imageLessThan(const ImageInfo &i1, const ImageInfo &i2) {
    if (i1.timeStamp < i2.timeStamp) {
        return true;
    } else if (i1.timeStamp > i2.timeStamp) {
        return false;
    }

    // Else the two images have the same timestamp. Sort by image type
    QStringList order = Settings::getInstance().imageTypeSortOrder();

    int type1 = order.contains(i1.imageTypeCode.toUpper()) ?
                order.indexOf(i1.imageTypeCode.toUpper()) : INT_MAX;
    int type2 = order.contains(i2.imageTypeCode.toUpper()) ?
                order.indexOf(i2.imageTypeCode.toUpper()) : INT_MAX;

    // The later the image type appears in the list the higher its index. And the
    // higher its index the lower its priority
    if (type1 > type2) {
        return true;
    } else if (type1 < type2) {
        return false;
    }

    // Else the two images have the same timestamp and the same image type
    // sort order. Try the title
    if (i1.title < i2.title) {
        return true;
    } else if (i1.title > i2.title) {
        return false;
    }

    // Nope. Description?
    // Else the two images have the same timestamp and the same image type
    // sort order. Try the title
    if (i1.description < i2.description) {
        return true;
    } else if (i1.description > i2.description) {
        return false;
    }

    // ID then
    return i1.id < i2.id;
}


bool imageGreaterThan(const ImageInfo &i1, const ImageInfo &i2) {
    return !imageLessThan(i1, i2);
}
