module;

#include "utils/jni_helper.hpp"

export module jni_helper;

export {
    using ::jboolean;
    using ::jbooleanArray;
    using ::jbyte;
    using ::jbyteArray;
    using ::jchar;
    using ::jcharArray;
    using ::jclass;
    using ::jdouble;
    using ::jdoubleArray;
    using ::jfieldID;
    using ::jfloat;
    using ::jfloatArray;
    using ::jint;
    using ::jintArray;
    using ::jlong;
    using ::jlongArray;
    using ::jmethodID;
    using ::JNIEnv;
    using ::jobject;
    using ::jobjectArray;
    using ::jshort;
    using ::jshortArray;
    using ::jsize;
    using ::jstring;
    using ::jthrowable;
    using ::jvalue;
}

export namespace lsplant {
using lsplant::JNIMonitor;
using lsplant::JNIScopeFrame;
using lsplant::JUTFString;
using lsplant::ScopedLocalRef;

using lsplant::UnwrapScope;
using lsplant::WrapScope;

using lsplant::JNI_GetBooleanField;
using lsplant::JNI_GetByteField;
using lsplant::JNI_GetCharField;
using lsplant::JNI_GetDoubleField;
using lsplant::JNI_GetFieldID;
using lsplant::JNI_GetFloatField;
using lsplant::JNI_GetIntField;
using lsplant::JNI_GetLongField;
using lsplant::JNI_GetObjectField;
using lsplant::JNI_GetShortField;
using lsplant::JNI_SetBooleanField;
using lsplant::JNI_SetByteField;
using lsplant::JNI_SetCharField;
using lsplant::JNI_SetDoubleField;
using lsplant::JNI_SetFloatField;
using lsplant::JNI_SetIntField;
using lsplant::JNI_SetLongField;
using lsplant::JNI_SetObjectField;
using lsplant::JNI_SetShortField;

using lsplant::JNI_GetStaticBooleanField;
using lsplant::JNI_GetStaticByteField;
using lsplant::JNI_GetStaticCharField;
using lsplant::JNI_GetStaticDoubleField;
using lsplant::JNI_GetStaticFieldID;
using lsplant::JNI_GetStaticFloatField;
using lsplant::JNI_GetStaticIntField;
using lsplant::JNI_GetStaticLongField;
using lsplant::JNI_GetStaticObjectField;
using lsplant::JNI_GetStaticShortField;
using lsplant::JNI_SetStaticBooleanField;
using lsplant::JNI_SetStaticByteField;
using lsplant::JNI_SetStaticCharField;
using lsplant::JNI_SetStaticDoubleField;
using lsplant::JNI_SetStaticFloatField;
using lsplant::JNI_SetStaticIntField;
using lsplant::JNI_SetStaticLongField;
using lsplant::JNI_SetStaticObjectField;
using lsplant::JNI_SetStaticShortField;

using lsplant::JNI_CallBooleanMethod;
using lsplant::JNI_CallByteMethod;
using lsplant::JNI_CallCharMethod;
using lsplant::JNI_CallDoubleMethod;
using lsplant::JNI_CallFloatMethod;
using lsplant::JNI_CallIntMethod;
using lsplant::JNI_CallLongMethod;
using lsplant::JNI_CallNonvirtualBooleanMethod;
using lsplant::JNI_CallNonvirtualByteMethod;
using lsplant::JNI_CallNonvirtualCharMethod;
using lsplant::JNI_CallNonvirtualDoubleMethod;
using lsplant::JNI_CallNonvirtualFloatMethod;
using lsplant::JNI_CallNonvirtualIntMethod;
using lsplant::JNI_CallNonvirtualLongMethod;
using lsplant::JNI_CallNonvirtualObjectMethod;
using lsplant::JNI_CallNonvirtualShortMethod;
using lsplant::JNI_CallNonvirtualVoidMethod;
using lsplant::JNI_CallObjectMethod;
using lsplant::JNI_CallShortMethod;
using lsplant::JNI_CallStaticBooleanMethod;
using lsplant::JNI_CallStaticByteMethod;
using lsplant::JNI_CallStaticCharMethod;
using lsplant::JNI_CallStaticDoubleMethod;
using lsplant::JNI_CallStaticFloatMethod;
using lsplant::JNI_CallStaticIntMethod;
using lsplant::JNI_CallStaticLongMethod;
using lsplant::JNI_CallStaticObjectMethod;
using lsplant::JNI_CallStaticShortMethod;
using lsplant::JNI_CallStaticVoidMethod;
using lsplant::JNI_CallVoidMethod;
using lsplant::JNI_GetMethodID;
using lsplant::JNI_GetStaticMethodID;
using lsplant::JNI_ToReflectedMethod;
using lsplant::JNI_ToReflectedField;

using lsplant::JNI_NewBooleanArray;
using lsplant::JNI_NewByteArray;
using lsplant::JNI_NewCharArray;
using lsplant::JNI_NewDirectByteBuffer;
using lsplant::JNI_NewDoubleArray;
using lsplant::JNI_NewFloatArray;
using lsplant::JNI_NewIntArray;
using lsplant::JNI_NewLongArray;
using lsplant::JNI_NewObject;
using lsplant::JNI_NewObjectArray;
using lsplant::JNI_NewShortArray;

using lsplant::JNI_Cast;
using lsplant::JNI_FindClass;
using lsplant::JNI_GetArrayLength;
using lsplant::JNI_GetObjectClass;
using lsplant::JNI_GetObjectFieldOf;
using lsplant::JNI_IsInstanceOf;
using lsplant::JNI_IsSameObject;
using lsplant::JNI_NewGlobalRef;
using lsplant::JNI_NewStringUTF;
using lsplant::JNI_RegisterNatives;
}  // namespace lsplant
