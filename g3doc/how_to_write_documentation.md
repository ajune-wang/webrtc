# How to write WebRTC documentation

<?% config.freshness.owner = 'titovartem' %?>
<?% config.freshness.reviewed = '2021-03-01' %?>

## Audience

Engineers and tech writers who wants to contribute to WebRTC documentation

## Conceptual documentation

Conceptual documentation provides overview of APIs or systems. Examples can
be threading model of a particular module or data life cycle. Conceptual 
documentation can skip some edge cases in favor of clarity. The main point
is to impart understanding.

Conceptual documentation often cannot be embedded directly within the source
code because it usually describes multiple APIs and entites, so the only
logical place to document such complex behavior is through a separate 
conceptual document.

A concept document needs to be useful to both experts and novices. Moreover,
it needs to emphasize clarity, so it often needs to sacrifice completeness
and sometimes stric accuracy. That's not to say a conceptual document should
intentionally be inaccurate. It just means that is should focus more on common
usage and leave rare ones or side effects for class/function level comments.

In WebRTC repo conceptual documentation is located in `g3doc` subfolders of
related components. To add a new document for the component `Foo` find a 
`g3doc` subfolder for this component and create an `.md` file there with
desired documentation. If there is no `g3doc` subfolder - create a new one
and add `g3doc.lua` file there with following content:

```
config = require('/g3doc/g3doc.lua')
return config
```

If you are a Googlers also please specify an owner, who will be responsible for
keeping this documentation updated, by adding next lines directly into `.md`
file:

```
<?\%- config.freshness.owner = '<user name>' -%?>
<?\%- config.freshness.reviewed = '<last review date in format yyyy-mm-dd>' -%?>
```

After the document is ready you should add it into `/g3doc/sitemap.md`, so it
will be visible for others.

