// Copyright (c) 2018, Peter Ohler, All rights reserved.

#include "error_stream.h"
#include "text.h"

static VALUE	es_class = Qundef;

typedef struct _ErrorStream {
    Server	server;
    Text	text;
} *ErrorStream;

static void
es_free(void *ptr) {
    ErrorStream	es = (ErrorStream)ptr;

    free(es->text); // allocated with malloc
    xfree(ptr);
}

VALUE
error_stream_new(Server server) {
    ErrorStream	es = ALLOC(struct _ErrorStream);

    es->text = text_allocate(1024);
    es->server = server;
    
    return Data_Wrap_Struct(es_class, NULL, es_free, es);
}

/* Document-method: puts

 * call-seq: puts(str)
 *
 * Write the _str_ to the stream along with a newline character, accumulating
 * it until _flush_ is called.
 */
static VALUE
es_puts(VALUE self, VALUE str) {
    ErrorStream	es = (ErrorStream)DATA_PTR(self);

    es->text = text_append(es->text, StringValuePtr(str), (int)RSTRING_LEN(str));
    es->text = text_append(es->text, "\n", 1);

    return Qnil;
}

/* Document-method: write

 * call-seq: write(str)
 *
 * Write the _str_ to the stream, accumulating it until _flush_ is called.
 */
static VALUE
es_write(VALUE self, VALUE str) {
    ErrorStream	es = (ErrorStream)DATA_PTR(self);
    int		cnt = (int)RSTRING_LEN(str);
    
    es->text = text_append(es->text, StringValuePtr(str), cnt);

    return INT2NUM(cnt);
}

/* Document-method: flush

 * call-seq: flush()
 *
 * Flushs the accumulated text in the stream as an error log entry.
 */
static VALUE
es_flush(VALUE self) {
    ErrorStream	es = (ErrorStream)DATA_PTR(self);

    log_cat(&es->server->error_cat, "%s", es->text->text);
    es->text->len = 0;

    return self;
}

/* Document-class: Agoo::ErrorStream
 *
 * Used in a reqquest as the _rack.errors_ attribute. Writing to the stream
 * and flushing will make an error log entry.
 */
void
error_stream_init(VALUE mod) {
    es_class = rb_define_class_under(mod, "ErrorStream", rb_cObject);

    rb_define_method(es_class, "puts", es_puts, 1);
    rb_define_method(es_class, "write", es_write, 1);
    rb_define_method(es_class, "flush", es_flush, 0);
}
