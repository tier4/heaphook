^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Changelog for package heaphook
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

0.1.0 (2023-05-23)
------------------
* ci: add build-and-test workflow (`#2 <https://github.com/tier4/heaphook/issues/2>`_)
  * add build-and-test workflow
  * Update .github/workflows/build-and-test.yaml
  Co-authored-by: Kenji Miyake <31987104+kenji-miyake@users.noreply.github.com>
  * Update build-and-test.yaml
  * fix linter error
  * fix
  * fix
  * fix
  ---------
  Co-authored-by: Kenji Miyake <31987104+kenji-miyake@users.noreply.github.com>
* style: apply lint (`#1 <https://github.com/tier4/heaphook/issues/1>`_)
  * apply lint
  * refactor package.xml
  ---------
* Update CMakeLists.txt
* Update package.xml
* Update package.xml metadata
* Create LICENSE
* Update README.md
* Create README.md
* Prevent infinit loop when additional mempool is not enough
* Enable to configure mempool size
* Delete unnecessary code
* Hanble exhaustion of memory pool
* Aquire lock for tlsf library
* Fix bug
* Add hooks for specialized malloc functions
* Deal with alignment related functions
* Fix
* Introduce tlsf allocator
* Create ament_cmake package
* Add malloc_usable_size hook
* Add aligned allocation hooks
* Add realloc hook
* Add calloc hook
* Safe figure as pdf in heaplog parser
* Add progress bar to heaplog parser
* Add python heaplog parser
* Change log format
* Complete heaplog parser
* Add mmaped log parser base
* Fix bug (prevent logging in logging thread)
* Make logging incremental and async
* Log malloc/free history
* Hook free
* First malloc hook
* Contributors: Daisuke Nishimatsu, Takahiro Ishikawa
